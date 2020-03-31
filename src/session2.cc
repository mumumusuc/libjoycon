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

#include "session2.h"
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

using namespace session;

static uint8_t _timer;
#define TIMER                                     \
    ({                                            \
        _timer = _timer > 0xfe ? 0 : _timer += 1; \
        _timer;                                   \
    })
#define msleep(ms) std::this_thread::sleep_for(std::chrono::milliseconds((ms)))

inline Semaphore::Semaphore(unsigned count) {
    sem_init(&sem_, 0, count);
}

inline Semaphore::~Semaphore() {
    sem_destroy(&sem_);
}

inline void Semaphore::Wait() {
    int ret = sem_wait(&sem_);
    assert(ret == 0);
}

inline bool Semaphore::TryWait() {
    int ret = sem_trywait(&sem_);
    if (ret == EAGAIN) return false;
    assert(ret == 0);
    return true;
}

inline void Semaphore::Post() {
    int ret = sem_post(&sem_);
    assert(ret == 0);
}

inline bool Semaphore::Valid() {
    int value = 0;
    int ret = sem_getvalue(&sem_, &value);
    assert(ret == 0);
    return value > 0;
}

inline Task::Task() : retry_(0), inspector_(nullptr){};

inline std::future<Result> Task::Reset(unsigned int retry, const Inspector &cb) {
    promise_ = std::promise<Result>();
    retry_ = retry;
    inspector_ = cb;
    return promise_.get_future();
}

inline void Task::Done() { promise_.set_value(DONE); }

inline void Task::Abort() { promise_.set_value(ABORT); };

inline void Task::Error() { promise_.set_value(ERROR); };

inline bool Task::Test(const void *buffer) {
    int ret = 0;
    if (retry_-- < 1) {
        promise_.set_value(TIMEDOUT);
        return true;
    }
    if (!inspector_) /* no one cares */
        return true;
    switch (ret = inspector_(buffer)) {
    case DONE:
        promise_.set_value(DONE);
        return true;
    case AGAIN:
        retry_++;
        return false;
    case WAITING:
        return false;
    default:
        Error();
        return true;
    }
}

inline TaskPool::TaskPool() : exported_(0) {}

inline TaskPool::~TaskPool() {
    std::lock_guard<std::mutex> _1(lock_);
    assert(exported_ == 0);
    size_t size = pool_.size();
    while (!pool_.empty()) {
        Task *t = pool_.front();
        pool_.pop();
        delete t;
    }
}

inline TaskSp TaskPool::Get() {
    Task *p = nullptr;
    std::lock_guard<std::mutex> _1(lock_);
    if (pool_.empty()) {
        p = new Task();
    } else {
        p = pool_.front();
        pool_.pop();
    }
    exported_++;
    auto deleter = [this](Task *t) {
        std::lock_guard<std::mutex> _1(lock_);
        pool_.emplace(t);
        exported_--;
    };
    return TaskSp(p, deleter);
}

Session::Session(const DeviceFunc *remote)
    : is_alive_(true), err_count_(0), poll_running_(false), push_running_(false), push_type_(FREE), push_sem_(1) {
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
            if (push_type_ == TIMED) {
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
}

Session::~Session() {
    debug("destroy session");
    int ret = 0;
    is_alive_ = false;
    if (poll_running_) {
#ifdef __android__
        ret = pthread_kill(tr_poll_, SIGQUIT);
        assert(ret == 0);
#else
        ret = pthread_cancel(tr_poll_);
        assert(ret == 0);
#endif
        debug("cancel poll thread done");
    }
    if (push_running_) {
#ifdef __android__
        ret = pthread_kill(tr_push_, SIGQUIT);
        assert(ret == 0);
#else
        ret = pthread_cancel(tr_push_);
        assert(ret == 0);
#endif
        debug("cancel push thread done");
    }
    if (poll_running_) {
        ret = pthread_join(tr_poll_, nullptr);
        assert(ret == 0);
        debug("join poll thread done");
    }
    if (push_running_) {
        ret = pthread_join(tr_push_, nullptr);
        assert(ret == 0);
        debug("join push thread done");
    }
    // clean task queue
    if (!task_queue_.empty()) {
        auto it = task_queue_.cbegin();
        while (it != task_queue_.cend()) {
            auto spp = it++;
            (*spp)->Abort();
            task_queue_.erase(spp);
        }
    }
    free(recv_buffer_);
    free(send_buffer_);
    debug("destroy session done");
}

inline ssize_t Session::Send(const void *buffer) {
    int ret = 0;
    if (remote_.sender) {
        reinterpret_cast<uint8_t *>(const_cast<void *>(buffer))[1] = TIMER;
        //hex_d("SEND", buffer, remote_.send_size);
        ret = remote_.sender(buffer, remote_.send_size);
        //debug("client send -> %ld", ret);
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

void *Session::Poll() {
    poll_running_ = true;
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
    while (is_alive_) {
        ret = Recv(recv_buffer_);
        if (ret < 0) {
            err_count_++;
            debug("recv error %d, err_count %d", ret, err_count_);
            if (err_count_ > 100) {
                debug("over 100 times error occurred, dozing...");
                msleep(100);
            }
        } else {
            std::lock_guard<std::mutex> _1(task_lock_);
            if (task_queue_.empty()) continue;
            auto it = task_queue_.cbegin();
            while (it != task_queue_.cend()) {
                auto spp = it++;
                if ((*spp)->Test(recv_buffer_))
                    task_queue_.erase(spp);
            }
        }
    }
    debug("exit poll thread ...");
    poll_running_ = false;
    pthread_exit(NULL);
    return NULL;
}

void *Session::Push() {
    push_running_ = true;
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
    while (is_alive_) {
        switch (push_type_) {
        case FREE:
            debug("I am not should not be here");
            break;
        case TIMED:
            if (!push_sem_.Valid()) {
                ret = Send(send_buffer_);
                if (ret < 0) {
                    err_count_++;
                    debug("send error %d, err_count %d", ret, err_count_);
                    if (err_count_ > 100) {
                        debug("over 100 times error occurred, dozing...");
                        msleep(100);
                    }
                }
                push_sem_.Post();
            }
            msleep(16);
            break;
        default:
            break;
        }
    }
    push_running_ = false;
    debug("exit push thread ...");
    pthread_exit(NULL);
    return NULL;
}

inline void Session::Append(TaskSp &&task) {
    if (!is_alive_ /*|| !poll_running_*/) {
        task->Abort();
        return;
    }
    std::lock_guard<std::mutex> _1(task_lock_);
    task_queue_.emplace_back(std::move(task));
}

std::future<Result> Session::Transmit(unsigned int retry, const void *buffer, const Inspector &inspector) {
    //debug();
    int ret = 0;
    auto sp = task_pool_.Get();
    auto future = sp->Reset(retry, inspector);
    if (!is_alive_) goto abort;
    if (buffer) {
        if (push_type_ == FREE) {
            ret = Send(buffer);
            if (ret < 0) sp->Error();
        } else {
            push_sem_.Wait();
            if (!is_alive_) goto abort;
            memmove(send_buffer_, buffer, remote_.send_size);
        }
    }
    if (inspector)
        Append(std::move(sp));
    else
        sp->Done();
    goto done;

abort:
    sp->Abort();
done:
    return future;
}
