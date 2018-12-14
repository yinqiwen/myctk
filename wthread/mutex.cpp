/*
 * mutex.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */
#include "mutex.h"
#include <atomic>
#include <errno.h>

namespace wthread
{
    struct MutexInternal
    {
            std::atomic<uint8_t> locked;
            std::atomic<uint8_t> contended;
            uint16_t padding;
    };
    const MutexInternal MUTEX_CONTENDED_RAW = { { 1 }, { 1 }, 0 };
    const MutexInternal MUTEX_LOCKED_RAW = { { 1 }, { 0 }, 0 };

#define WTHREAD_MUTEX_LOCKED  (*(const uint32_t*)&MUTEX_LOCKED_RAW)
#define WTHREAD_MUTEX_CONTENDED (*(const uint32_t*)&MUTEX_CONTENDED_RAW)

    inline int mutex_lock_contended(wthread_mutex_t* m)
    {
        std::atomic<uint32_t>* whole = (std::atomic<uint32_t>*) m->mutex;
        while (whole->exchange(WTHREAD_MUTEX_CONTENDED) & WTHREAD_MUTEX_LOCKED)
        {
            if (wthread::wutex_wait(whole, WTHREAD_MUTEX_CONTENDED, NULL) < 0 && errno != EWOULDBLOCK
                    && errno != EINTR/*note*/)
            {
                // a mutex lock should ignore interrruptions in general since
                // user code is unlikely to check the return value.
                return errno;
            }
        }
        return 0;
    }


}

extern "C"
{
    int wthread_mutex_lock(wthread_mutex_t* mutex)
    {
        wthread::MutexInternal* split = (wthread::MutexInternal*) mutex->mutex;
        if (!split->locked.exchange(1, std::memory_order_acquire))
        {
            return 0;
        }
        // NOTE: Don't modify m->csite outside lock since multiple threads are
        // still contending with each other.
        const int rc = wthread::mutex_lock_contended(mutex);
        return rc;
    }

    int wthread_mutex_unlock(wthread_mutex_t* m)
    {
        std::atomic<uint32_t>* whole = (std::atomic<uint32_t>*)m->mutex;
        const uint32_t prev = whole->exchange(0, std::memory_order_release);
        // CAUTION: the mutex may be destroyed, check comments before butex_create
        if (prev == WTHREAD_MUTEX_LOCKED)
        {
            return 0;
        }
//        // Wakeup one waiter
//        if (!bthread::is_contention_site_valid(saved_csite))
//        {
//            bthread::butex_wake(whole);
//            return 0;
//        }
//        const int64_t unlock_start_ns = butil::cpuwide_time_ns();
//        bthread::butex_wake(whole);
//        const int64_t unlock_end_ns = butil::cpuwide_time_ns();
//        saved_csite.duration_ns += unlock_end_ns - unlock_start_ns;
//        bthread::submit_contention(saved_csite, unlock_end_ns);
        return 0;
    }
}

