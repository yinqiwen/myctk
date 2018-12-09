/*
 * mythread.h
 *
 *  Created on: Dec 8, 2018
 *      Author: wangqiying
 */

#ifndef MYTHREAD_MYTHREAD_H_
#define MYTHREAD_MYTHREAD_H_

#include <stdint.h>

#if __cplusplus
extern "C" {
#endif
typedef uint64_t mthread_t;
int init_mythread(int concurrency);
int mythread_start_urgent(mthread_t* __restrict tid, void * (*fn)(void*),
		void* __restrict args);

int mythread_start_background(mthread_t* __restrict tid, void * (*fn)(void*),
		void* __restrict args);

void mythread_join(mthread_t tid);
void mythread_interrupt(mthread_t tid);
void mythread_resume(mthread_t tid);
void mythread_sleep(int ms);

mthread_t mythread_self();

#if __cplusplus
}
#endif

#endif /* MYTHREAD_MYTHREAD_H_ */
