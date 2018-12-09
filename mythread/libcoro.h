#ifndef COROXX_LIBCORO_H_
#define COROXX_LIBCORO_H_

#include "coro_base.h"
#include "coro.h"
#include <atomic>
#include <vector>

namespace mythread
{
    struct LibCoroCoroutine;
    struct CoroStack
    {
            coro_stack* stack;
            LibCoroCoroutine* coro;
            bool shared;
            CoroStack(uint32_t stack_size, bool share)
                    : stack(NULL), coro(NULL),shared(share)
            {
                stack = (coro_stack*) malloc(sizeof(coro_stack));
                coro_stack_alloc(stack, stack_size);
                //memset(stack->sptr, 0, stack->ssze);
            }
            ~CoroStack()
            {
                //printf("##delete stack!\n");
                if (NULL != stack)
                {
                    coro_stack_free(stack);
                    free(stack);
                }
            }
    };

    class Worker;
    struct CoroTask {
    	MythreadFunction func;
    	mthread_t mid;
    	LibCoroCoroutine* coro;
    	Worker* worker;
    	std::atomic<bool> interrupt;
    	std::vector<mthread_t> join_list;
    	CoroTask(const MythreadFunction& f, mthread_t id) :
    			func(f), mid(id), coro(NULL),worker(NULL),interrupt(false) {
    	}
    	~CoroTask();
    };

    struct LibCoroCoroutine
    {
            coro_context ctx;
            CoroTask* task;
            coro_stack stack;
            LibCoroCoroutine(CoroTask* task);
            ~LibCoroCoroutine();

            void Resume(LibCoroCoroutine* current);
    };
}
#endif /* COROXX_LIBCORO_H_ */
