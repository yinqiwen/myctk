/*
 * wthread.h
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_WTHREAD_H_
#define WTHREAD_WTHREAD_H_

#include "types.h"
#include <time.h>
#include <sys/time.h>

#if __cplusplus
extern "C"
{
#endif

    enum ErrWthreadCode
    {
        WTHREAD_ERR_INVALID = -1,

    };

    int init_wthread(int concurrency);
    int wthread_start_urgent(wthread_t* __restrict tid, void * (*fn)(void*), void* __restrict args);

    int wthread_start_background(wthread_t* __restrict tid, void * (*fn)(void*), void* __restrict args);

    void wthread_yield(wthread_t tid);
    void wthread_join(wthread_t tid);
    void wthread_interrupt(wthread_t tid);
    void wthread_resume(wthread_t tid);
    void wthread_sleep(int ms);

    wthread_t wthread_self();

//    int wthread_mutex_init(wthread_mutex_t* __restrict mutex);
//    int wthread_mutex_destroy(wthread_mutex_t* mutex);
//    // Wait until lock for `mutex' becomes available and lock it.
//    int wthread_mutex_lock(wthread_mutex_t* mutex);
//    // Wait until lock becomes available and lock it or time exceeds `abstime'
//    int wthread_mutex_timedlock(wthread_mutex_t* __restrict mutex, const struct timespec* __restrict abstime);
//    // Unlock `mutex'.
//    int wthread_mutex_unlock(wthread_mutex_t* mutex);



#if __cplusplus
}
#endif

#endif /* WTHREAD_WTHREAD_H_ */
