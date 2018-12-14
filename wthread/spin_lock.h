/*
 * spin_lock.h
 *
 *  Created on: Dec 12, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_SPIN_LOCK_H_
#define WTHREAD_SPIN_LOCK_H_

#include <atomic>
#include <sched.h>
#include <stdint.h>

namespace wthread
{
    class SpinMutexLock
    {
        public:
            std::atomic<bool> _lock;
        public:
            SpinMutexLock()
                    : _lock(false)
            {
            }
            bool lock()
            {
                bool expected = false;
                while (!_lock.compare_exchange_strong(expected, true))
                    sched_yield();
                return true;
            }
            bool unlock()
            {
                _lock.store(false);
                return true;
            }
            ~SpinMutexLock()
            {
            }
    };

//    class SpinRWLock
//    {
//        public:
//            std::atomic<uint32_t> _lock;
//        public:
//            SpinRWLock()
//                    : _lock(0)
//            {
//            }
//            bool lock_read()
//            {
//                for (;;)
//                {
//                    // Wait for active writer to release the lock
//                    while (m_lock & 0xfff00000)
//                        sched_yield();
//
//                    if ((0xfff00000 & atomic_add_uint32(&m_lock, 1)) == 0) return true;
//
//                    atomic_sub_uint32(&m_lock, 1);
//                }
//                return true;
//            }
//
//            bool lock_write()
//            {
//
//                for (;;)
//                {
//                    // Wait for active writer to release the lock
//                    while (m_lock & 0xfff00000)
//                        sched_yield();
//
//                    if ((0xfff00000 & atomic_add_uint32(&m_lock, 0x100000)) == 0x100000)
//                    {
//                        // Wait until there's no more readers
//                        while (m_lock & 0x000fffff)
//                            sched_yield();
//
//                        return true;
//                    }
//                    atomic_sub_uint32(&m_lock, 0x100000);
//                }
//                return true;
//            }
//            bool unlock()
//            {
//                switch (mode)
//                {
//                    case READ_LOCK:
//                    {
//                        atomic_sub_uint32(&m_lock, 1);
//                        return true;
//                    }
//                    case WRITE_LOCK:
//                    {
//                        atomic_sub_uint32(&m_lock, 0x100000);
//                        return true;
//                    }
//                    default:
//                    {
//                        return false;
//                    }
//                }
//            }
//            ~SpinRWLock()
//            {
//            }
//    };
}

#endif /* WTHREAD_SPIN_LOCK_H_ */
