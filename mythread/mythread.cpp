/*
 * mythread.cpp
 *
 *  Created on: Dec 8, 2018
 *      Author: wangqiying
 */
#include "mythread.h"
#include "sched.h"
#include <pthread.h>

extern "C" {

static mythread::Scheduler* g_sched = NULL;

int init_mythread(int concurrency) {
	mythread::SchedulerOptions options;
	options.concurrency = concurrency;
	g_sched = new mythread::Scheduler;
	g_sched->init(options);
	return 0;
}

int mythread_start_urgent(mthread_t* __restrict tid, void * (*fn)(void*),
		void* __restrict args) {

	auto func = [fn, args]()
	{
		fn(args);
	};

	mthread_t id = g_sched->start_task(func, true);
	if (NULL != tid) {
		*tid = id;
	}
	return 0;
}

int mythread_start_background(mthread_t* __restrict tid, void * (*fn)(void*),
		void* __restrict args) {
	auto func = [fn, args]()
	{
		fn(args);
	};

	mthread_t id = g_sched->start_task(func, false);
	if (NULL != tid) {
		*tid = id;
	}
	return 0;
}

void mythread_join(mthread_t tid) {
	if (NULL != g_sched) {
		g_sched->join(tid);
	}
}
mthread_t mythread_self() {
	return mythread::mythread_self();
}
void mythread_interrupt(mthread_t tid) {
	if (NULL != g_sched) {
		g_sched->interrupt(tid);
	}
}
void mythread_resume(mthread_t tid) {
	if (NULL != g_sched) {
		g_sched->resume(tid);
	}
}
void mythread_sleep(int ms) {

}
}

namespace mythread {
mthread_t start_task(const MythreadFunction& task, bool urgent) {
	return g_sched->start_task(task, urgent);
}
}

