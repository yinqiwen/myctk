/*
 * utex.cpp
 *
 *  Created on: Dec 13, 2018
 *      Author: qiyingwang
 */
#include "utex.h"
#include <atomic>
#include <deque>
#include <stdint.h>
#include <mutex>
#include "spin_lock.h"
#include "worker.h"

namespace wthread
{
    struct Wutex;
    struct WutexWaiter
    {
            // tids of pthreads are 0
            wthread_t tid;

            // Erasing node from middle of LinkedList is thread-unsafe, we need
            // to hold its container's lock.
            std::atomic<Wutex*> container;
            uint32_t expected_value;
            WutexWaiter()
                    : tid(0), expected_value(0)
            {
            }
    };
    typedef std::deque<WutexWaiter*> WutexWaiterQueue;
    struct Wutex
    {
            std::atomic<uint32_t> value;
            WutexWaiterQueue waiters;
            SpinMutexLock waiter_lock;
    };

    void* wutex_create()
    {
        Wutex* w = new Wutex;
        return w;
    }
    void wutex_destroy(void* wutex)
    {
        Wutex* w = (Wutex*) wutex;
        delete w;
    }

    int wutex_wake(void* wutex)
    {
        Wutex* w = (Wutex*) wutex;
        WutexWaiter* front = NULL;
        {
            std::unique_lock<SpinMutexLock> guard(w->waiter_lock);
            if (w->waiters.empty())
            {
                return 0;
            }
            front = w->waiters.front();
            w->waiters.pop_front();
            front->container.store(NULL, std::memory_order_relaxed);
        }
        if (front->tid == 0)
        {
            //wakeup_pthread(static_cast<ButexPthreadWaiter*>(front));
            return 1;
        }
        wthread_resume(front->tid);
//        ButexBthreadWaiter* bbw = static_cast<ButexBthreadWaiter*>(front);
//        unsleep_if_necessary(bbw, get_global_timer_thread());
//        TaskGroup* g = tls_task_group;
//        if (g)
//        {
//            TaskGroup::exchange(&g, bbw->tid);
//        }
//        else
//        {
//            bbw->control->choose_one_group()->ready_to_run_remote(bbw->tid);
//        }
        return 1;
    }

    int wutex_wake_all(void* wutex)
    {
        Wutex* w = (Wutex*) wutex;
        WutexWaiterQueue waiters;
        WutexWaiter* front = NULL;
        {
            std::unique_lock<SpinMutexLock> guard(w->waiter_lock);
            if (w->waiters.empty())
            {
                return 0;
            }
            for (auto waiter : w->waiters)
            {
                waiter->container.store(NULL, std::memory_order_relaxed);
                waiters.push_back(waiter);
            }
            w->waiters.clear();
        }
        if (front->tid == 0)
        {
            //wakeup_pthread(static_cast<ButexPthreadWaiter*>(front));
            return 1;
        }
        for (auto waiter : waiters)
        {
            wthread_resume(waiter->tid);
        }
        int n = waiters.size();
        return n;
    }

    int wutex_wait(void* wutex, uint32_t expected_value, const timespec* abstime)
    {
        Wutex* w = (Wutex*) wutex;
        if (w->value.load(std::memory_order_relaxed) != expected_value)
        {
            errno = EWOULDBLOCK;
            // Sometimes we may take actions immediately after unmatched butex,
            // this fence makes sure that we see changes before changing butex.
            std::atomic_thread_fence(std::memory_order_acquire);
            return -1;
        }
        WutexWaiter bbw;
        // tid is 0 iff the thread is non-bthread
        bbw.tid = wthread_self();
        bbw.container.store(NULL, std::memory_order_relaxed);
        bbw.expected_value = expected_value;

        auto func = [&bbw, w]()
        {
            {
                std::unique_lock<SpinMutexLock> guard(w->waiter_lock);
                if (w->value.load(std::memory_order_relaxed) != bbw.expected_value)
                {
                    wthread_resume(bbw.tid);
                    //bw->waiter_state = WAITER_STATE_UNMATCHEDVALUE;
                }
                else
                {
                    w->waiters.push_back(&bbw);
                    //bw->container.store(b, butil::memory_order_relaxed);
                    return;
                }
            }
        };
        tls_worker()->set_necessary_task(func);
//        bbw.task_meta = g->current_task();
//        bbw.sleep_id = 0;
//        bbw.waiter_state = WAITER_STATE_READY;
//        bbw.expected_value = expected_value;
//        bbw.initial_butex = b;
//        bbw.control = g->control();

        wthread_interrupt(bbw.tid);
        return 0;
    }

}

