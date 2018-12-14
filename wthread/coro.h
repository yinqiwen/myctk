/*
 * coro.h
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_CORO_H_
#define WTHREAD_CORO_H_

#include <atomic>
#include <vector>
#include <functional>
#include "wthread.h"
#include "coctx.h"
#include "mutex.h"

namespace wthread
{

    typedef std::function<void(void)> WthreadFunction;

    class Worker;
    class Coroutine;

    struct CoroStack
    {
            void* stack_mem;
            size_t stack_size;
            CoroStack()
                    : stack_mem(NULL), stack_size(0)
            {
            }
    };

    enum CoroState{
        CORO_EMPTY = 0,
        CORO_QUEUED = 1,
        CORO_RUNNING,
        CORO_INTERRUPT,
        CORO_STOPPED
    };

    struct CoroTask
    {
            WthreadFunction func;
            wthread_t tid;
            Coroutine* coro;
            Worker* worker;
            std::atomic<uint32_t>* seq_mutex;
            std::atomic<uint8_t> state;
            int64_t create_ns;
            int64_t start_ns;
            int64_t interrupt_ns;
            CoroTask();
            ~CoroTask();
    };

    class Coroutine
    {
        private:
            coctx_t _ctx;
            CoroTask* _task;
            CoroStack _stack;
        public:
            Coroutine(CoroTask* task);
            void resume(Coroutine* current);
            ~Coroutine();
    };
}

#endif /* WTHREAD_CORO_H_ */
