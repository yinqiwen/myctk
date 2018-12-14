/*
 * worker.h
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_WORKER_H_
#define WTHREAD_WORKER_H_
#include "coro.h"
#include <pthread.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <set>
#include <functional>
#include <unordered_map>
#include <condition_variable>

namespace wthread
{
    struct WthreadOptions
    {
            int concurrency;
            int max_wthreads;
            WthreadOptions()
                    : concurrency(1), max_wthreads(10000)
            {
            }
    };
    typedef std::deque<CoroTask*> CoroTaskQueue;
    class WorkerGroup;
    class Worker
    {
        private:
            static void* work_func(void* data);
            pthread_t _tid;
            WorkerGroup* _worker_group;
            bool _running;
            Coroutine* _main_coro;
            CoroTask* _running_task;
            CoroTaskQueue _thread_task_queue;
            CoroTaskQueue _finished_tasks;
            std::mutex _tasks_mutex;
            std::condition_variable _tasks_cond;
            uint32_t _running_coro_num;
            WthreadFunction _necessary_task;
            friend class Scheduler;
            friend class LibCoroCoroutine;
            void run();
            void coro_gc();
            bool get_task(CoroTask** task);
            void exec_necessary_task();
        public:
            Worker(WorkerGroup* s);
            void finish(CoroTask* task);
            void sched(CoroTask* task);
            CoroTask* current_task()
            {
                return _running_task;
            }
            uint32_t get_coro_num()
            {
                return _running_coro_num;
            }
            void requeue(CoroTask* t);
            void interrupt(CoroTask* t);
            void set_necessary_task(const WthreadFunction& next)
            {
                exec_necessary_task();
                _necessary_task = next;
            }
    };

    class WorkerGroup
    {
        private:
            std::vector<CoroTask> _all_task_pool;
            CoroTaskQueue _free_task_queue;
            std::vector<Worker*> _workers;
            CoroTaskQueue _pendding_task_queue;
            std::mutex _running_task_mutex;
            typedef std::unordered_map<wthread_t, CoroTask*> CoroTaskTable;
            CoroTaskTable _running_tasks;
            std::atomic<uint32_t> _seq
            void finish_task(CoroTask* task);
            CoroTask* take_task(Worker *w);
            friend class Worker;
            CoroTask* get_coro_task(wthread_t tid);
        public:
            WorkerGroup();
            int init(const WthreadOptions& options);
            wthread_t start(const WthreadFunction& func, bool urgent);
            int join(wthread_t id);
            int interrupt(wthread_t id);
            int resume(wthread_t id);
    };

    Worker* tls_worker();
}

#endif /* WTHREAD_WORKER_H_ */
