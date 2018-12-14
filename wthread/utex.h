/*
 * utex.h
 *
 *  Created on: Dec 13, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_UTEX_H_
#define WTHREAD_UTEX_H_
#include "wthread.h"
#include <time.h>

namespace wthread
{
    // Create a butex which is a futex-like 32-bit primitive for synchronizing
    // bthreads/pthreads.
    // Returns a pointer to 32-bit data, NULL on failure.
    // NOTE: all butexes are private(not inter-process).
    void* wutex_create();

    // Destroy the butex.
    void wutex_destroy(void* wutex);

    // Wake up at most 1 thread waiting on |butex|.
    // Returns # of threads woken up.
    int wutex_wake(void* wutex);

    // Wake up all threads waiting on |butex|.
    // Returns # of threads woken up.
    int wutex_wake_all(void* wutex);

//    // Wake up all threads waiting on |butex| except a bthread whose identifier
//    // is |excluded_bthread|. This function does not yield.
//    // Returns # of threads woken up.
//    int wutex_wake_except(void* wutex, wthread_t excluded_bthread);

    // Wake up at most 1 thread waiting on |butex1|, let all other threads wait
    // on |butex2| instead.
    // Returns # of threads woken up.
    int wutex_requeue(void* wutex1, void* wutex2);

    // Atomically wait on |butex| if *butex equals |expected_value|, until the
    // butex is woken up by butex_wake*, or CLOCK_REALTIME reached |abstime| if
    // abstime is not NULL.
    // About |abstime|:
    //   Different from FUTEX_WAIT, butex_wait uses absolute time.
    // Returns 0 on success, -1 otherwise and errno is set.
    int wutex_wait(void* wutex, uint32_t expected_value, const timespec* abstime);
}

#endif /* WTHREAD_UTEX_H_ */
