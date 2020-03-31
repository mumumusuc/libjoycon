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
#include <semaphore.h>
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

namespace session {
class Task;
using Inspector = std::function<int(const void *)>;
using Deleter = std::function<void(Task *)>;
using TaskSp = std::unique_ptr<Task, Deleter>;

enum Result {
    DONE = 0,
    WAITING,
    AGAIN = EAGAIN,
    TIMEDOUT = ETIMEDOUT,
    ABORT = ECANCELED,
    ERROR,
};

enum PushType {
    FREE,
    TIMED,
};

class Task {
  private:
    int retry_;
    std::promise<Result> promise_;
    Inspector inspector_;

  public:
    explicit Task();
    std::future<Result> Reset(unsigned int, const Inspector &);
    void Done();
    void Abort();
    void Error();
    bool Test(const void *);
};

class TaskPool {
  private:
    int exported_;
    std::mutex lock_;
    std::queue<Task *> pool_;

  public:
    explicit TaskPool();
    ~TaskPool();
    TaskSp Get();
};

class Semaphore {
  private:
    sem_t sem_;

  public:
    explicit Semaphore(unsigned);
    ~Semaphore();
    void Wait();
    bool TryWait();
    void Post();
    bool Valid();
};

class Session {
  private:
    bool is_alive_;
    int err_count_;
    DeviceFunc remote_;
    void *recv_buffer_;
    void *send_buffer_;
    std::list<TaskSp> task_queue_;
    std::mutex task_lock_;
    TaskPool task_pool_;
    pthread_t tr_poll_;
    pthread_t tr_push_;
    bool poll_running_;
    bool push_running_;
    PushType push_type_;
    Semaphore push_sem_;
    void *Poll();
    void *Push();
    void Append(TaskSp &&);
    ssize_t Send(const void *);
    ssize_t Recv(void *);

  public:
    explicit Session(const DeviceFunc *);
    ~Session();
    std::future<Result> Transmit(unsigned int, const void *, const Inspector &);
};

}; // namespace session

#endif // __cplusplus
#endif // SESSION_H