/*
 * sched.cpp
 *
 *  Created on: Dec 8, 2018
 *      Author: wangqiying
 */
#include "sched.h"
#include <errno.h>

namespace mythread {

static __thread Worker* g_current_worker = NULL;

static CoroTask* tls_curent_task() {
	if (NULL == g_current_worker) {
		return NULL;
	}
	return g_current_worker->current_task();
}

Worker::Worker(Scheduler* s) :
		_tid(0), _sched(s), _running(true), _main_coro(
		NULL), _running_task(NULL), _last_yield_task(NULL), _running_coro_num(0) {

}

void* Worker::work_func(void* data) {
	Worker* w = (Worker*) data;
	w->run();
	return NULL;
}

void Worker::coro_gc() {
	for (size_t i = 0; i < _finished_tasks.size(); i++) {
		delete _finished_tasks[i];
	}
	_finished_tasks.clear();
}

void Worker::finish_task(CoroTask* task) {
//	if(task == _running_task)
//	{
//		_running_task = NULL;
//	}
	_finished_tasks.push_back(task);
	_running_coro_num--;
	_sched->finish_task(task);
}
void Worker::sched(CoroTask* task) {
//	if(NULL != task)
//	{
//		printf("###sched to  %d %p\n", task->mid,task->coro);
//	}

	CoroTask* current = _running_task;
	_running_task = task;
	if (NULL == task) {
		_main_coro->Resume(current->coro);
		//task->coro->Resume(_main_coro);
		return;
	} else {
		if (NULL == task->coro) {
			task->worker = this;
			task->coro = new LibCoroCoroutine(task);
			_running_coro_num++;
		}
	}
	//printf("###sched to  %d %p\n", task->mid,task->coro);
	task->interrupt = false;
	if (NULL == current) {
		task->coro->Resume(_main_coro);
	} else {
		task->coro->Resume(current->coro);
	}
	_running_task = current;
}

void Worker::yield_task(CoroTask* next) {
	if (!_running_task->interrupt) {
		if (NULL != _last_yield_task) {
			requeue(_running_task);
		} else {
			_last_yield_task = _running_task;
		}
	}
	sched(next);
}

bool Worker::wait_task(CoroTask** task) {
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		if (!_task_queue.empty()) {
			CoroTask* v = _task_queue.front();
			_task_queue.pop_front();
			*task = v;
			return true;
		}
	}
	*task = _sched->take_task(this);
	return (*task) != NULL;
}
void Worker::requeue(CoroTask* t) {
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		_task_queue.push_back(t);
	}
	if (g_current_worker != this) {
		_tasks_cond.notify_one();
	}
}
void Worker::interrupt(CoroTask* t) {
	if (t->interrupt) {
		return;
	}
	t->interrupt = true;
	yield_task(NULL);
}

void Worker::run() {
	g_current_worker = this;
	_main_coro = new LibCoroCoroutine(NULL);
	while (_running) {
		coro_gc();
		CoroTask* task = NULL;
		if (wait_task(&task)) {
			sched(task);
		}

		while (NULL != _last_yield_task) {
			CoroTask* c = _last_yield_task;
			_last_yield_task = NULL;
			sched(c);
		}
	}
	g_current_worker = NULL;
	//delete _main_coro;
}

Scheduler::Scheduler() :
		_mythread_id_seed(0) {

}

int Scheduler::init(const SchedulerOptions& options) {
	int err = 0;
	for (int i = 0; i < options.concurrency; i++) {
		Worker* w = new Worker(this);
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if (0 != pthread_create(&w->_tid, &attr, Worker::work_func, w)) {
			err = errno;
			delete w;
			break;
		}
		_workers.push_back(w);
	}
	return err;
}

CoroTask* Scheduler::take_task(Worker *w) {
	CoroTask* task = NULL;
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		if ((_tasks.empty())) {
			w->_tasks_cond.wait_for(lk, std::chrono::microseconds(10));
		}
		if (!_tasks.empty()) {
			task = _tasks.front();
			_tasks.pop_front();
		}
	}
	return task;
}
mthread_t Scheduler::start_task(const MythreadFunction& task, bool urgent) {
	bool should_urgent = false;
	if (NULL != g_current_worker && urgent) {
		should_urgent = true;
	}
	mthread_t id;
	CoroTask* coro_task = NULL;
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		_mythread_id_seed++;
		id = _mythread_id_seed;
		coro_task = new CoroTask(task, id);
		_running_tasks[id] = coro_task;
		if (!should_urgent) {
			_tasks.push_back(coro_task);
		}
	}
	if (!should_urgent) {
		//_tasks_cond.notify_one();
		Worker* selected_worker = NULL;
		uint32_t min_coro_num = 0;
		for (size_t i = 0; i < _workers.size(); i++) {
			if (0 == _workers[i]->get_coro_num()) {
				selected_worker = _workers[i];
				break;
			}
		}
		selected_worker->_tasks_cond.notify_one();

	} else {
		g_current_worker->yield_task(coro_task);
	}
	return id;
}

void Scheduler::finish_task(CoroTask* task) {
	CoroTask* next_task = NULL;
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		_running_tasks.erase(task->mid);
		for (size_t i = 0; i < task->join_list.size(); i++) {
			CoroTask* wake = NULL;
			CoroTaskTable::iterator found = _running_tasks.find(
					task->join_list[i]);
			if (found != _running_tasks.end()) {
				wake = found->second;
			}
			if (NULL != wake) {
				wake->interrupt = false;
				if (NULL == next_task && wake->worker == g_current_worker
						&& wake != task) {
					next_task = wake;
				} else {
					wake->worker->requeue(wake);
				}
			}
		}
	}

	if (NULL != next_task) {
		next_task->worker->sched(next_task);
	} else {
		task->worker->sched(NULL);
		//task->coro->Yield();
	}
}

void Scheduler::join(mthread_t id) {
	CoroTask* running_task = NULL;
	CoroTask* interrupt_task = tls_curent_task();
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		CoroTaskTable::iterator found = _running_tasks.find(id);
		if (found != _running_tasks.end()) {
			running_task = found->second;
		}
	}

	if (NULL != running_task) {
		if (NULL != interrupt_task) {
			g_current_worker->interrupt(interrupt_task);
		}
	}
}
void Scheduler::interrupt(mthread_t id) {
	CoroTask* interrupt_task = tls_curent_task();
	if (NULL == interrupt_task || interrupt_task->mid != id) {
		return;
	}
	g_current_worker->interrupt(interrupt_task);
}
void Scheduler::resume(mthread_t id) {
	CoroTask* interrupt_task = NULL;
	{
		std::unique_lock<std::mutex> lk(_tasks_mutex);
		CoroTaskTable::iterator found = _running_tasks.find(id);
		if (found != _running_tasks.end()) {
			interrupt_task = found->second;
		}
	}
	if(NULL != interrupt_task && interrupt_task->interrupt)
	{
		interrupt_task->interrupt = true;
		interrupt_task->worker->requeue(interrupt_task);
	}
}

mthread_t mythread_self() {
	if (NULL != g_current_worker && NULL != g_current_worker->current_task()) {
		return g_current_worker->current_task()->mid;
	}
	return 0;
}
}

