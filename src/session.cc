/*
 *   Copyright (c) 2020 mumumusuc

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "session.h"
#include "log.h"
#include <assert.h>
#include <errno.h>

#ifdef __android__

#include <signal.h>

#endif

#define DEBUG 1
#if DEBUG
#define debug(fmt, ...) log_d(__func__, fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

static uint8_t _timer;
#define TIMER                                     \
    ({                                            \
        _timer = _timer > 0xfe ? 0 : _timer += 1; \
        _timer;                                   \
    })
#define msleep(ms) std::this_thread::sleep_for(std::chrono::milliseconds((ms)))

using Result = Session::Result;
using Inspector = Session::Inspector;

class Session::Task {
  private:
    int retry_;
    std::promise<Result> promise_;

  public:
    Inspector inspector;

    explicit Task() : retry_(0), inspector(nullptr){};

    void reset(std::promise<Result> &&promise, unsigned int timeout, Inspector cb) {
        promise_ = std::move(promise);
        retry_ = timeout;
        inspector = cb;
    }

    std::future<Result> reset(unsigned int retry, Inspector cb) {
        promise_ = std::promise<Result>();
        retry_ = retry;
        inspector = cb;
        return promise_.get_future();
    }

    //void set_value(Result result) { promise_.set_value(result); };
    void done() { promise_.set_value(DONE); }

    void abort() { promise_.set_value(ABORT); };

    void error(int code) { promise_.set_value(ERROR(code)); };

    bool test(const void *buffer) {
        int ret = 0;
        if (retry_-- < 1) {
            promise_.set_value(TIMEDOUT);
            return true;
        }
        if (!inspector)
            // no one cares
            return true;
        switch (ret = inspector(buffer)) {
        case DONE:
            promise_.set_value(DONE);
            return true;
        case AGAIN:
            retry_++;
            return false;
        case WAITING:
            return false;
        default:
            promise_.set_value(ERROR(ret));
            return true;
        }
    }
};

Session::Session(const DeviceFunc *remote) : is_alive_(true), task_pool_(), push_type_(FREE), push_sem_(1) {
    int ret = 0;
    debug("create session");
    if (remote) {
        remote_ = *const_cast<DeviceFunc *>(remote);
        if (remote_.recver) {
            recv_buffer_ = calloc(1, remote_.recv_size);
            if (recv_buffer_ == NULL)
                throw std::runtime_error(strerror(ENOMEM));
            // start poll thread
            auto poll = [](void *arg) -> void * {
                assert(arg);
                auto sess = reinterpret_cast<Session *>(arg);
                return sess->Poll();
            };
            ret = pthread_create(&tr_poll_, NULL, std::move(poll), this);
            assert(ret == 0);
        }
        if (remote_.sender) {
            send_buffer_ = calloc(1, remote_.send_size);
            if (send_buffer_ == NULL) {
                free(recv_buffer_);
                throw std::runtime_error(strerror(ENOMEM));
            }
            // start push thread
            auto push = [](void *arg) -> void * {
                assert(arg);
                auto sess = reinterpret_cast<Session *>(arg);
                return sess->Push();
            };
            ret = pthread_create(&tr_push_, NULL, std::move(push), this);
            assert(ret == 0);
        }
    }
}

Session::~Session() {
    debug("destroy session");
    int ret = 0;
    is_alive_ = false;
    if (tr_poll_running_) {
#ifdef __android__
        ret = pthread_kill(tr_poll_, SIGQUIT);
        assert(ret == 0);
#else
        ret = pthread_cancel(tr_poll_);
        assert(ret == 0);
#endif
        debug("cancel poll thread done");
    }
    if (tr_push_running_) {
        push_cond_.notify_one();
#ifdef __android__

#else
        ret = pthread_cancel(tr_push_);
        assert(ret == 0);
#endif
        debug("cancel push thread done");
    }
    if (tr_poll_running_) {
        ret = pthread_join(tr_poll_, nullptr);
        assert(ret == 0);
        debug("join poll thread done");
    }
    if (tr_push_running_) {
        ret = pthread_join(tr_push_, nullptr);
        assert(ret == 0);
        debug("join push thread done");
    }
    free(recv_buffer_);
    free(send_buffer_);
    debug("destroy session done");
}

inline ssize_t Session::Send(const void *buffer) {
    int ret = 0;
    if (remote_.sender) {
        if (push_sem_.valid()) {
            debug("push_sem is valid, refuse to send");
            return -EINVAL;
        }
        reinterpret_cast<uint8_t *>(const_cast<void *>(buffer))[1] = TIMER;
        //hex_d("SEND", buffer, remote_.send_size);
        ret = remote_.sender(buffer, remote_.send_size);
        //debug("client send -> %ld", ret);
        push_sem_.post();
        return ret;
    }
    return -1;
}

inline ssize_t Session::Recv(void *buffer) {
    if (remote_.recver) {
        //hex_d("RECV", buffer, remote_.recv_size);
        return remote_.recver(buffer, remote_.recv_size);
    }
    return -1;
}

void Session::PollClean() {
    int ret = 0;
    debug("clean poll thread");
    // clean recv queue
    poll_task_queue_.for_each([this](TaskSp sp) {
        sp->abort();
        poll_task_queue_.remove(sp);
    });
}

void *Session::Poll() {
    tr_poll_running_ = true;
    int ret = 0;
#ifdef __android__
    signal(SIGQUIT, [](int signo) {
        pthread_exit(nullptr);
    });
#else
    ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    assert(ret == 0);
    ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    assert(ret == 0);
#endif
    debug("enter poll thread ...");
    pthread_cleanup_push([](void *arg) {
        assert(arg);
        auto sess = reinterpret_cast<Session *>(arg);
        sess->PollClean();
    },
                         this);
    // barrier_.wait();
    // assert(ret == 0);
    while (is_alive_) {
        //pthread_testcancel();
        ret = Recv(recv_buffer_);
        if (ret < 0)
            debug("recv error %d", ret);
        else
            poll_task_queue_.for_each([this](TaskSp sp) {
                if (sp->test(recv_buffer_))
                    poll_task_queue_.remove(sp);
            });
        // msleep(4);
    }
    debug("exit poll loop");
    pthread_cleanup_pop(0);
    debug("exit poll thread ...");
    tr_poll_running_ = false;
    pthread_exit(NULL);
    return NULL;
}

void Session::PushClean() {
    int ret = 0;
    debug("clean push thread");
    push_sem_.abort();
}

void *Session::Push() {
    tr_push_running_ = true;
    int ret = 0;
#ifdef __android__
    signal(SIGQUIT, [](int signo) {
        pthread_exit(nullptr);
    });
#else
    ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    assert(ret == 0);
    ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    assert(ret == 0);
#endif
    debug("enter push thread ...");
    pthread_cleanup_push([](void *arg) {
        assert(arg);
        auto sess = reinterpret_cast<Session *>(arg);
        sess->PushClean();
    },
                         this);
    while (is_alive_) {
        //pthread_testcancel();
        // HOW TO PUSH
        // Timer :
        //  0. lock buffer
        //  1. send_sem == 0?
        //  Y:  2.1 update timer
        //      2.2 send & sem_post
        //  N:  2.1 call update send buffer
        //      2.2 update timer
        //      2.3 send
        //  3. unlock buffer
        //  4. sleep 16ms
        // ASYNC :
        //  0. lock buffer
        //  1. send_sem == 0?
        //  Y:  2.1 send & post
        //  3. unlock buffer
        //  4. wait-> send_sem == 0
        // IDEL :
        //  0. just sleep until push_type changed or session destroy
        switch (push_type_) {
        case FREE: {
            std::unique_lock<std::mutex> lock(push_mutex_);
            if (!push_sem_.valid() && push_task_queue_.size() > 0) {
                auto sp = std::move(push_task_queue_.front());
                push_task_queue_.pop();
                ret = Send(send_buffer_);
                if (ret < 0)
                    sp->error(ret);
                if (sp->inspector)
                    AppendPollTask(std::move(sp));
                else
                    sp->done();
            }
            push_cond_.wait(lock, [this]() { return !is_alive_ || !push_sem_.valid(); });
            break;
        }
        case TIMED: {
            std::lock_guard<std::mutex> lock(push_mutex_);
            if (push_sem_.valid()) {
                push_sem_.wait();
                // callback update
            } else {
                //push_sem_.post();
            }
            auto sp = std::move(push_task_queue_.front());
            push_task_queue_.pop();
            ret = Send(send_buffer_);
            if (ret < 0)
                sp->error(ret);
            if (sp->inspector)
                poll_task_queue_.append(std::move(sp));
            else
                sp->done();
        }
            msleep(16);
            break;
        default:
            break;
        }
    }
    debug("exit push loop");
    pthread_cleanup_pop(0);
    tr_push_running_ = false;
    debug("exit push thread ...");
    pthread_exit(NULL);
    return NULL;
}

inline void Session::AppendPollTask(TaskSp &&task) {
    if (!is_alive_ || !tr_poll_running_) {
        task->abort();
        return;
    }
    poll_task_queue_.append(task);
}

inline void Session::AppendPushTask(TaskSp &&task) {
    if (!is_alive_ || !tr_push_running_) {
        task->abort();
        return;
    }
    push_task_queue_.emplace(task);
    if (push_type_ == FREE)
        push_cond_.notify_one();
}

std::future<Result>
Session::Transmit(unsigned int retry, const void *buffer, const Inspector &inspector, bool async) {
    //debug();
    int ret = 0;
    auto sp = task_pool_.get();
    auto future = sp->reset(retry, inspector);
    if (!is_alive_) {
        sp->abort();
        goto done;
    }

    if (buffer) {
        if (!push_sem_.wait()) {
            sp->abort();
            goto done;
        }
        memmove(send_buffer_, buffer, remote_.send_size);
        if (async || push_type_ == TIMED) {
            std::lock_guard<std::mutex> lock(push_mutex_);
            AppendPushTask(std::move(sp));
        } else {
            ret = Send(send_buffer_);
            if (ret < 0)
                sp->error(ret);
            if (sp->inspector)
                AppendPollTask(std::move(sp));
            else
                sp->done();
            push_sem_.post();
        }
    } else if (inspector)
        AppendPollTask(std::move(sp));
    else
        sp->done();

done:
    return future;
}
