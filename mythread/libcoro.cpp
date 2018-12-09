#include "libcoro.h"
#include <stack>
#include <vector>
#include <set>
#include <string.h>
#include <mutex>
#include "sched.h"

namespace mythread {

static std::mutex g_create_mutex;

CoroTask::~CoroTask() {
	printf("###delete %d %d\n",mid, pthread_self());
	delete coro;
}

static void LibCoroFunc(void* data) {
	CoroTask* task = (CoroTask*) data;
	(task->func)();
	task->worker->finish_task(task);
}

LibCoroCoroutine::LibCoroCoroutine(CoroTask* t) :
		task(t) {
	std::unique_lock<std::mutex> lk(g_create_mutex);
	if (NULL == task) {
		coro_create(&ctx, NULL, NULL, NULL, 0);
	} else {
		coro_stack_alloc(&stack, 0);
		coro_create(&ctx, LibCoroFunc, task, stack.sptr, stack.ssze);
	}
}

LibCoroCoroutine::~LibCoroCoroutine() {
	coro_destroy(&ctx);
	coro_stack_free(&stack);
}


void LibCoroCoroutine::Resume(LibCoroCoroutine* current) {
	coro_transfer(&(current->ctx), &(this->ctx));
}
}
