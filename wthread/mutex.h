/*
 * mutext.h
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_MUTEX_H_
#define WTHREAD_MUTEX_H_

#include "wthread.h"

namespace wthread
{
    class Mutex
    {
        public:
            typedef wthread_mutex_t* native_handler_type;
            Mutex()
            {
                wthread_mutex_init(&_mutex);
            }
            ~Mutex()
            {
                wthread_mutex_destroy(&_mutex);
            }
            native_handler_type native_handler()
            {
                return &_mutex;
            }
            void lock()
            {
                wthread_mutex_lock(&_mutex);
            }
            void unlock()
            {
                wthread_mutex_unlock(&_mutex);
            }
            bool try_lock()
            {
                //return !bthread_mutex_trylock(&_mutex);
            }
        private:
            wthread_mutex_t _mutex;
    };


}

#endif /* WTHREAD_MUTEX_H_ */
