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

#ifndef SESSION_H
#define SESSION_H

#include "device.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus

#include "tools.hpp"
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

extern "C" {
#endif
// c
typedef unsigned int timeout_t;
typedef struct Session Session;
typedef struct SendTask SendTask;
#define CONST_PTR const *const
#ifdef __cplusplus
}
// c++

class Session {
  public:
    using Inspector = std::function<int(const void *)>;

    typedef enum Result {
        DONE = 0,
        WAITING,
        AGAIN = EAGAIN,
        TIMEDOUT = -ETIMEDOUT,
        ABORT = -ECANCELED,
#define ERROR(err) static_cast<Result>((err))
    } result_t;

    typedef enum PushType {
        FREE,
        TIMED,
    } push_type_t;

  private:
    class Task;
    class TaskPool;
    using TaskSp = std::shared_ptr<Task>;

    bool is_alive_;
    int err_count_;
    DeviceFunc remote_;
    AsyncQueue<TaskSp> poll_task_queue_;
    std::queue<TaskSp> task_queue_;
    std::mutex task_lock_;
    std::queue<TaskSp> push_task_queue_;
    ObjectPoolShared<Task> task_pool_;
    void *recv_buffer_;
    void *send_buffer_;
    pthread_t tr_poll_;
    pthread_t tr_push_;
    bool tr_poll_running_ = false;
    bool tr_push_running_ = false;
    PushType push_type_;
    QueuedSem push_sem_;
    std::mutex push_mutex_;
    std::condition_variable push_cond_;

    void AppendPollTask(TaskSp &&);
    void AppendPushTask(TaskSp &&);
    void *Poll();
    void *Push();
    void PollClean();
    void PushClean();
    ssize_t Send(const void *);
    ssize_t Recv(void *);

  public:
    explicit Session(const DeviceFunc *);
    ~Session();
    std::future<Result>
    Transmit(unsigned int retry, const void *buffer, const Inspector &inspector, bool async);
};

#endif // __cplusplus
#endif // SESSION_H