/*
 * types.h
 *
 *  Created on: Dec 11, 2018
 *      Author: qiyingwang
 */

#ifndef WTHREAD_TYPES_H_
#define WTHREAD_TYPES_H_
#include <stdint.h>

#if __cplusplus
extern "C"
{
#endif
    typedef uint64_t wthread_t;
    typedef struct
    {
            unsigned* mutex;
            //bthread_contention_site_t csite;
    } wthread_mutex_t;



#if __cplusplus
}
#endif

#endif /* WTHREAD_TYPES_H_ */
