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

#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

typedef std::unique_lock<std::mutex> UNIQUE_LOCK;
typedef std::lock_guard<std::mutex> GUARD_LOCK;
#define THIS_THREAD std::this_thread::get_id()
#define creater_t std::function<T *()>
#define deleter_t std::function<void(T *)>
#define recycler_t std::function<void(T *)>

template <typename T>
class ObjectPool {
#define OBJ_POOL_DEBUG 0
#if OBJ_POOL_DEBUG
#define obj_printf(fmt, ...) func_printf(fmt, ##__VA_ARGS__)
#else
#define obj_printf(fmt, ...)
#endif
  private:
    int _exported = 0;
    std::mutex _lock;
    std::queue<T *> _pool;
    const creater_t _creater;
    const deleter_t _deleter;

  protected:
    recycler_t _recycler;
    T *obtain();

  public:
    explicit ObjectPool(creater_t, deleter_t);
    ~ObjectPool();
};

template <typename T>
inline T *ObjectPool<T>::obtain() {
    T *p = nullptr;
    GUARD_LOCK lock(_lock);
    if (_pool.empty()) {
        if (_creater)
            p = _creater();
        else
            p = new T();
        obj_printf("use new pointer -> %p", p);
    } else {
        p = _pool.front();
        _pool.pop();
        obj_printf("use cache pointer -> %p", p);
    }
    _exported++;
    return p;
}

template <typename T>
inline ObjectPool<T>::ObjectPool(creater_t creater, deleter_t deleter)
    : _creater(creater), _deleter(deleter) {
    _recycler = [this](T *t) {
        GUARD_LOCK lock(_lock);
        obj_printf("recycle pointer -> %p", t);
        _pool.emplace(t);
        _exported--;
    };
};

template <typename T>
inline ObjectPool<T>::~ObjectPool() {
    GUARD_LOCK lock(_lock);
    assert(_exported == 0);
    size_t size = _pool.size();
    obj_printf("have %lu cache to free", size);
    while (!_pool.empty()) {
        T *t = _pool.front();
        _pool.pop();
        obj_printf("deleting pointer -> %p", t);
        if (_deleter)
            _deleter(t);
        else
            delete t;
    }
}

#define OBTAIN ObjectPool<T>::obtain()
#define RECYCLER ObjectPool<T>::_recycler
template <typename T>
class ObjectPoolShared : public ObjectPool<T> {
#define SP_T std::shared_ptr<T>
  public:
    explicit ObjectPoolShared() : ObjectPool<T>(nullptr, nullptr){};
    explicit ObjectPoolShared(creater_t creater, deleter_t deleter)
        : ObjectPool<T>(creater, deleter){};
    SP_T get() { return SP_T(OBTAIN, RECYCLER); };
#undef SP_T
};

template <typename T>
class ObjectPoolUnique : public ObjectPool<T> {
#define SP_T std::unique_ptr<T, recycler_t>
#define OBTAIN ObjectPool<T>::obtain()
  public:
    explicit ObjectPoolUnique(creater_t creater, deleter_t deleter)
        : ObjectPool<T>(creater, deleter){};
    SP_T get() { return SP_T(OBTAIN, RECYCLER); };
#undef SP_T
};
#undef RECYCLER
#undef OBTAIN

#undef creater_t
#undef deleter_t
#undef recycler_t

class QueuedSem {
  private:
    bool _abort;
    int _value;
    std::mutex _lock;
    std::condition_variable _cond;
    std::queue<std::thread::id> _queue;

  public:
    explicit QueuedSem(unsigned);
    ~QueuedSem();
    void post();
    bool wait();
    bool valid();
    void abort();
};

inline QueuedSem::QueuedSem(unsigned value) : _abort(false), _value(value){};

inline QueuedSem::~QueuedSem() {
    while (!_queue.empty()) {
        _queue.pop();
    }
}

inline void QueuedSem::post() {
    {
        GUARD_LOCK lock(_lock);
        ++_value;
    }
    _cond.notify_all();
};

inline bool QueuedSem::wait() {
    UNIQUE_LOCK lock(_lock);
    if (!valid()) {
        _queue.emplace(THIS_THREAD);
        _cond.wait(lock, [this]() {
            return _abort || (_value > 0 && _queue.front() == THIS_THREAD);
        });
        _queue.pop();
    }
    --_value;
    return !_abort;
};

inline bool QueuedSem::valid() { return _value > 0; };

inline void QueuedSem::abort() {
    GUARD_LOCK lock(_lock);
    _abort = true;
    _cond.notify_all();
};

#define ITERATOR std::list<T>::iterator

class SharedLock : public std::mutex {
  private:
    std::thread::id _locker;
    const std::thread::id _THREAD_NULL = std::thread::id();

  public:
    void lock();
    void unlock();
};

inline void SharedLock::lock() {
    if (_locker != THIS_THREAD) {
        std::mutex::lock();
        _locker = THIS_THREAD;
    }
}

inline void SharedLock::unlock() {
    if (_locker == THIS_THREAD) {
        _locker = _THREAD_NULL;
        std::mutex::unlock();
    }
}

template <typename T>
class AsyncQueue {
  private:
    typedef std::lock_guard<SharedLock> SHARED_LOCK;
    std::list<T> _list;
    SharedLock _lock;

  public:
    ~AsyncQueue();
    unsigned int size();
    void append(T);
    void remove(T);
    T pop();
    bool for_each(std::function<void(T)>);
};

template <typename T>
inline AsyncQueue<T>::~AsyncQueue() {
    assert(_list.empty());
}

template <typename T>
inline unsigned int AsyncQueue<T>::size() {
    SHARED_LOCK lock(_lock);
    return _list.size();
}

template <typename T>
inline void AsyncQueue<T>::append(T t) {
    SHARED_LOCK lock(_lock);
    _list.emplace_back(t);
};

template <typename T>
inline void AsyncQueue<T>::remove(T t) {
    SHARED_LOCK lock(_lock);
    _list.remove(t);
};

template <typename T>
inline T AsyncQueue<T>::pop() {
    SHARED_LOCK lock(_lock);
    if (_list.empty())
        return NULL;
    T t = _list.front();
    _list.pop_front();
    return t;
}

template <typename T>
inline bool AsyncQueue<T>::for_each(std::function<void(T)> func) {
    SHARED_LOCK lock(_lock);
    if (!_list.empty()) {
        auto it = _list.begin();
        do {
            func(*(it++));
        } while (it != _list.end());
        return true;
    }
    return false;
}

class ThreadPool {
  public:
    ThreadPool(size_t);
    template <class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type>
    enqueue(F &&f, Args &&... args);
    ~ThreadPool();

  private:
    typedef std::function<void()> Callback;
    bool _stop;
    std::vector<std::thread> _workers;
    std::queue<Callback> _tasks;
    std::mutex _queue_mutex;
    std::condition_variable _condition;
};

inline ThreadPool::ThreadPool(size_t threads) : _stop(false) {
    for (size_t i = 0; i < threads; ++i)
        _workers.emplace_back([this] {
            for (;;) {
                Callback task;
                {
                    UNIQUE_LOCK lock(_queue_mutex);
                    _condition.wait(lock, [this] { return _stop || !_tasks.empty(); });
                    if (_stop && _tasks.empty())
                        return;
                    //task = std::move(_tasks.front());
                    task = _tasks.front();
                    _tasks.pop();
                }
                task();
            }
        });
}

template <class F, class... Args>
std::future<typename std::result_of<F(Args...)>::type>
ThreadPool::enqueue(F &&f, Args &&... args) {
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
        GUARD_LOCK lock(_queue_mutex);
        if (_stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        _tasks.emplace([task]() { (*task)(); });
    }
    _condition.notify_one();
    return res;
}

inline ThreadPool::~ThreadPool() {
    {
        UNIQUE_LOCK lock(_queue_mutex);
        _stop = true;
    }
    _condition.notify_all();
    for (std::thread &worker : _workers)
        worker.join();
}

#endif