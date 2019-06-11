/*
 *
 * BSD 3-Clause License
 *
 * Copyright (c) 2019, Taymindis Woon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _WFQ_H_
#define _WFQ_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WFQ_MAX_SPIN 1024

#if defined __GNUC__ || defined __APPLE__
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#define __WFQ_FETCH_ADD_ __sync_fetch_and_add
#define __WFQ_CAS_ __sync_bool_compare_and_swap
#define __WFQ_SWAP_ __sync_lock_test_and_set
#define __WFQ_THREAD_ID_ pthread_self
#define __WFQ_SYNC_MEMORY_ __sync_synchronize
#define __WFQ_YIELD_THREAD_ sched_yield
#else
#include <Windows.h>
#ifdef _WIN64
inline bool ___WFQ_CAS_64(LONG64 volatile *x, LONG64 y, LONG64 z) {
    return InterlockedCompareExchangeNoFence64(x, z, y) == y;
}
inline LONG __WFQ_InterlockedExchangeAddNoFence64(LONG64 volatile *target, LONG64 value) {
    return (LONG) InterlockedExchangeAddNoFence64(target, value);
}
inline LONG64 __WFQ_InterlockedExchange64(LONG64 volatile *target, LONG64 value) {
    return InterlockedExchange64(target, value);
}
#define __WFQ_CAS_(x,y,z)  ___WFQ_CAS_64((LONG64 volatile *)x, (LONG64)y, (LONG64)z)
#define __WFQ_FETCH_ADD_(target, value)  __WFQ_InterlockedExchangeAddNoFence64((LONG64 volatile *)target, (LONG64)value)
#define __WFQ_SWAP_(target, value) (VOID*)__WFQ_InterlockedExchange64((LONG64 volatile *)target, (LONG64)value)
#else
inline bool ___WFQ_CAS_(LONG volatile *x, LONG y, LONG z) {
    return InterlockedCompareExchangeNoFence(x, z, y) == y;
}
inline LONG __WFQ_InterlockedExchangeAddNoFence(LONG volatile *target, LONG value) {
    return InterlockedExchangeAddNoFence(target, value);
}
inline LONG __WFQ_InterlockedExchange(LONG volatile *target, LONG value) {
    return InterlockedExchange(target, value);
}
#define __WFQ_CAS_(x,y,z)  ___WFQ_CAS_((LONG volatile *)x, (LONG)y, (LONG)z)
#define __WFQ_FETCH_ADD_(target, value) (size_t)__WFQ_InterlockedExchangeAddNoFence((LONG volatile *)target, (LONG)value)
#define __WFQ_SWAP_(target, value) (VOID*)__WFQ_InterlockedExchange((LONG volatile *)target, (LONG)value)
#endif
// thread
#include <windows.h>
#define __WFQ_SYNC_MEMORY_ MemoryBarrier
#define __WFQ_YIELD_THREAD_ SwitchToThread
#define __WFQ_THREAD_ID_ GetCurrentThreadId
#endif

#include <assert.h>
/*
*
*  WFQ FIXED SIZE wait free queue, it is faster a bit but size are fixed
*
*/
// #define WFQ_UNSET_INDEX (size_t) -1

typedef struct {
    volatile size_t enq_barrier;
    volatile size_t count;
    volatile size_t head;
    volatile size_t tail;
    volatile size_t max;
    void **nptr;
} wfqueue_t;

static wfqueue_t *wfq_create(size_t fixed_size);
static int wfq_enq(wfqueue_t *q, void* val);
static void* wfq_deq(wfqueue_t *q);
static void wfq_destroy(wfqueue_t *q);

static wfqueue_t *
wfq_create(size_t fixed_size) {
    size_t i;
    wfqueue_t *q = (wfqueue_t *)malloc(sizeof(wfqueue_t));
    if (!q) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    q->enq_barrier = 0;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->nptr = (void**)malloc( fixed_size * sizeof(void*));

    if (!q->nptr) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < fixed_size; i++) {
        q->nptr[i] = NULL;
    }
    q->max = fixed_size;
    return q;
}

static int
wfq_enq(wfqueue_t *q, void* val) {
    size_t head, max;
    max = q->max;

    __WFQ_FETCH_ADD_(&q->enq_barrier, 1);
    __WFQ_SYNC_MEMORY_();
    if ( __WFQ_FETCH_ADD_(&q->count, 1) >= max ) {
      __WFQ_FETCH_ADD_(&q->count, -1);
    __WFQ_FETCH_ADD_(&q->enq_barrier, -1);
      return 0;
    } else {
        head = __WFQ_FETCH_ADD_(&q->head, 1);
        if (__WFQ_CAS_(q->nptr + (head%max), NULL, val)) {
          __WFQ_FETCH_ADD_(&q->enq_barrier, -1);
            return 1;
        }
//          __WFQ_FETCH_ADD_(&q->count, -1);
//        __WFQ_FETCH_ADD_(&q->enq_barrier, -1);
////        return 0;
//        printf("Exit ERRRRRRRRRRR");
//        exit(-1);
        assert(0 && "Error, incorrect number of head, it shouldn't reach here");
    }
    return 0;
}

static void*
wfq_deq(wfqueue_t *q) {
    size_t tail, max, cnt;
    void *val;
    max = q->max;

    cnt = __WFQ_FETCH_ADD_(&q->count, 0);
    __WFQ_SYNC_MEMORY_();

    if ( ((int)(cnt - __WFQ_FETCH_ADD_(&q->enq_barrier, 0))) > 0 ) {
        tail = __WFQ_FETCH_ADD_(&q->tail, 0);
        // tail %= max;
        if ( (val = __WFQ_SWAP_(q->nptr + (tail%max), NULL) ) ) {
            __WFQ_FETCH_ADD_(&q->tail, 1);
            __WFQ_FETCH_ADD_(&q->count, -1);
          return val;
        }
    }
    return NULL;
}

static void
wfq_destroy(wfqueue_t *q) {
    free(q->nptr);
    free(q);
}


static inline size_t
wfq_size(wfqueue_t *q) {
    return __WFQ_FETCH_ADD_(&q->count, 0);
}

static inline size_t
wfq_capacity(wfqueue_t *q) {
    return __WFQ_FETCH_ADD_(&q->max, 0);
}

#ifdef __cplusplus
}
#endif

#endif
