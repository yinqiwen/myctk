/*
 * worker.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */
#include "worker.h"
#include "utex.h"

namespace wthread
{
    static __thread Worker* _tls_worker = NULL;

    Worker* tls_worker()
    {
        return _tls_worker;
    }

    Worker::Worker(WorkerGroup* s)
            : _tid(0), _worker_group(s), _running(true), _main_coro(NULL), _running_task(NULL), _running_coro_num(0)
    {

    }
    void* Worker::work_func(void* data)
    {
        Worker* w = (Worker*) data;
        w->run();
        return NULL;
    }
    void Worker::finish(CoroTask* task)
    {

    }
    void Worker::coro_gc()
    {
        for (size_t i = 0; i < _finished_tasks.size(); i++)
        {
            delete _finished_tasks[i];
        }
        _finished_tasks.clear();
    }
    void Worker::requeue(CoroTask* t)
    {
        {
            std::unique_lock<std::mutex> lk(_tasks_mutex);
            _thread_task_queue.push_back(t);
        }
        if (_tls_worker != this)
        {
            _tasks_cond.notify_one();
        }
    }
    void Worker::interrupt(CoroTask* t)
    {
        if (t->worker == this)
        {
            t->interrupt = true;
            sched(NULL);
        }
    }
    bool Worker::get_task(CoroTask** task)
    {
        {
            std::unique_lock<std::mutex> lk(_tasks_mutex);
            if (!_thread_task_queue.empty())
            {
                CoroTask* v = _thread_task_queue.front();
                _thread_task_queue.pop_front();
                *task = v;
                return true;
            }
        }
        *task = _worker_group->take_task(this);
        return (*task) != NULL;
    }

    void Worker::exec_necessary_task()
    {
        while (!_necessary_task)
        {
            WthreadFunction f = _necessary_task;
            _necessary_task = 0;
            f();
        }
    }

    void Worker::sched(CoroTask* task)
    {
        CoroTask* current = _running_task;
        _running_task = task;
        if (NULL == task)
        {
            exec_necessary_task();
            _main_coro->resume(current->coro);
            return;
        }
        if (NULL == task->coro)
        {
            task->worker = this;
            task->coro = new Coroutine(task);
            _running_coro_num++;
        }
        task->interrupt = false;
        if (NULL == current)
        {
            task->coro->resume(_main_coro);
        }
        else
        {
            if (!current->interrupt)
            {
                requeue(current);
            }
            task->coro->resume(current->coro);
        }
        _running_task = current;
    }

    void Worker::run()
    {
        _tid = pthread_self();
        _tls_worker = this;
        _main_coro = new Coroutine(NULL);
        while (_running)
        {
            coro_gc();
            exec_necessary_task();
            CoroTask* task = NULL;
            if (get_task(&task))
            {
                sched(task);
            }
        }
        _tls_worker = NULL;
    }

    int WorkerGroup::init(const WthreadOptions& options)
    {
        _all_task_pool.resize(options.max_wthreads);
        for (int i = 0; i < options.max_wthreads; i++)
        {
            _all_task_pool[i].tid = i;
            _free_task_queue.push_back(&_all_task_pool[i]);
        }
        int err = 0;
        for (int i = 0; i < options.concurrency; i++)
        {
            Worker* w = new Worker(this);
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            if (0 != pthread_create(&w->_tid, &attr, Worker::work_func, w))
            {
                err = errno;
                delete w;
                break;
            }
            _workers.push_back(w);
        }
        return err;
    }

    wthread_t WorkerGroup::start(const WthreadFunction& func, bool urgent)
    {
        wthread_t tid;
        CoroTask* coro_task = NULL;
        bool should_urgent = false;
        if (NULL != _tls_worker && urgent)
        {
            should_urgent = true;
        }
        {
            std::unique_lock<std::mutex> lk(_running_task_mutex);
            if (_free_task_queue.empty())
            {
                return (wthread_t) -1;
            }
            coro_task = _free_task_queue.front();
            tid = coro_task->tid;
            coro_task->running_mutex->store(1, std::memory_order_release);
            //coro_task = new CoroTask(func, tid);
            _running_tasks[tid] = coro_task;
            if (!should_urgent)
            {
                _pendding_task_queue.push_back(coro_task);
            }
        }
        if (!should_urgent)
        {
            //_tasks_cond.notify_one();
            Worker* selected_worker = NULL;
            uint32_t min_coro_num = 0;
            for (size_t i = 0; i < _workers.size(); i++)
            {
                if (0 == _workers[i]->get_coro_num())
                {
                    selected_worker = _workers[i];
                    break;
                }
            }
            selected_worker->_tasks_cond.notify_one();
        }
        else
        {
            _tls_worker->sched(coro_task);
        }
        return tid;
    }

    void WorkerGroup::finish_task(CoroTask* task)
    {
        task->running_mutex->store(0, std::memory_order_release);
        wutex_wake_all(task->running_mutex);
        {
            std::unique_lock<std::mutex> lk(_running_task_mutex);
            _running_tasks.erase(task->tid);
            _free_task_queue.push_back(task);
        }
    }

    CoroTask* WorkerGroup::get_coro_task(wthread_t tid)
    {
        std::unique_lock<std::mutex> lk(_running_task_mutex);
        CoroTaskTable::iterator found = _running_tasks.find(tid);
        if (found != _running_tasks.end())
        {
            return found->second;
        }
        return NULL;
    }

    int WorkerGroup::join(wthread_t id)
    {
        CoroTask* running_task = get_coro_task(id);
        if (NULL != running_task && NULL != _tls_worker)
        {
            wutex_wait(running_task->version_mutex, 1, NULL);
            return 0;
        }
        return 0;
    }

    int WorkerGroup::interrupt(wthread_t id)
    {
        CoroTask* current_task = _tls_worker->current_task();
        if (NULL == _tls_worker || _tls_worker->current_task() != id)
        {
            return -1;
        }
        _tls_worker->interrupt(current_task);
        return 0;
    }
    int WorkerGroup::resume(wthread_t id)
    {
        CoroTask* interrupt_task = NULL;
        {
            std::unique_lock<std::mutex> lk(_running_task_mutex);
            CoroTaskTable::iterator found = _running_tasks.find(id);
            if (found != _running_tasks.end())
            {
                interrupt_task = found->second;
            }
        }
        if (NULL != interrupt_task && interrupt_task->interrupt)
        {
            interrupt_task->interrupt = false;
            interrupt_task->worker->requeue(interrupt_task);
        }
        return 0;
    }
}

