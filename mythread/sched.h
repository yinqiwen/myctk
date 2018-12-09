/*
 * sched.h
 *
 *  Created on: Dec 8, 2018
 *      Author: wangqiying
 */

#ifndef MYTHREAD_SCHED_H_
#define MYTHREAD_SCHED_H_

#include <pthread.h>
#include <stdint.h>
#include <vector>
#include <deque>
#include <set>
#include <functional>
#include <unordered_map>
#include <condition_variable>

#include "libcoro.h"

namespace mythread {

struct SchedulerOptions {
	int concurrency;
	SchedulerOptions() :
			concurrency(1) {
	}
};
typedef std::deque<CoroTask*> CoroTaskQueue;
class Scheduler;
class Worker {
private:
	static void* work_func(void* data);
	pthread_t _tid;
	Scheduler* _sched;
	bool _running;
	LibCoroCoroutine* _main_coro;
	CoroTaskQueue _finished_tasks;
	CoroTask* _running_task;
	CoroTask* _last_yield_task;
	CoroTaskQueue _task_queue;
	std::mutex _tasks_mutex;
	std::condition_variable _tasks_cond;
	uint32_t _running_coro_num;
	friend class Scheduler;
	friend class LibCoroCoroutine;
	void run();
	void coro_gc();
	bool wait_task(CoroTask** task);
public:
	Worker(Scheduler* s);
	void yield_task(CoroTask* task);
	void finish_task(CoroTask* task);
	void sched(CoroTask* task);
	CoroTask* current_task() {
		return _running_task;
	}
	uint32_t get_coro_num()
	{
		return _running_coro_num;
	}
	void requeue(CoroTask* t);
	void interrupt(CoroTask* t);
};

class Scheduler {
private:
	std::vector<Worker*> _workers;
	CoroTaskQueue _tasks;
	std::mutex _tasks_mutex;
	mthread_t _mythread_id_seed;
	typedef std::unordered_map<mthread_t, CoroTask*> CoroTaskTable;
	CoroTaskTable _running_tasks;
	void finish_task(CoroTask* task);

	friend class Worker;
public:
	Scheduler();
	int init(const SchedulerOptions& options);
	CoroTask* take_task(Worker *w);
	mthread_t start_task(const MythreadFunction& task, bool urgent);
	void join(mthread_t id);
	void interrupt(mthread_t id);
	void resume(mthread_t id);

};

mthread_t start_task(const MythreadFunction& task, bool urgent);

mthread_t mythread_self();

}

#endif /* MYTHREAD_SCHED_H_ */
