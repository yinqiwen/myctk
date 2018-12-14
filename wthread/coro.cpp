/*
 * coro.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */
#include "coro.h"
#include "worker.h"
#include "utex.h"

extern "C"
{
    extern void coctx_swap(coctx_t *, coctx_t*) asm("coctx_swap");
}

namespace wthread
{

    CoroTask::CoroTask()
            : tid(0), coro(NULL), worker(NULL), seq_mutex(NULL), create_ns(0), start_ns(0), interrupt_ns(0)
    {
        seq_mutex = (std::atomic<uint32_t>*)wutex_create();
        seq_mutex->store(0, std::memory_order_relaxed);
        state = CORO_EMPTY;
    }
    CoroTask::~CoroTask()
    {
        wutex_destroy(seq_mutex);
    }
    static int coro_func(void* co, void *)
    {
        CoroTask* task = (CoroTask*) co;
        task->func();
        //co->cEnd = 1;
        task->worker->finish(task);
        return 0;
    }
    Coroutine::Coroutine(CoroTask* task)
            : _task(task)
    {
        coctx_init(&_ctx);
        if (NULL != task)
        {
            _stack.stack_size = 128*1024;
            _stack.stack_mem = malloc(_stack.stack_size);
            _ctx.ss_sp = _stack.stack_mem;
            _ctx.ss_size = _stack.stack_size;
            coctx_make(&_ctx, (coctx_pfn_t) coro_func, task, 0);
        }
    }
    void Coroutine::resume(Coroutine* current)
    {
        coctx_swap(&(current->_ctx), &_ctx);
    }
    Coroutine::~Coroutine()
    {
        if(NULL !=  _stack.stack_mem)
        {
            free(_stack.stack_mem);
        }
    }
}

