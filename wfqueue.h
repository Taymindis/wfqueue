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
inline LONG64 __WFQ_InterlockedExchangeAddNoFence64(LONG64 volatile *target, LONG64 value) {
    return InterlockedExchangeAddNoFence64(target, value);
}
inline LONG64 __WFQ_InterlockedExchange64(LONG64 volatile *target, LONG64 value) {
    return InterlockedExchange64(target, value);
}
#define __WFQ_CAS_(x,y,z)  ___WFQ_CAS_64((LONG64 volatile *)x, (LONG64)y, (LONG64)z)
#define __WFQ_FETCH_ADD_(target, value)  __WFQ_InterlockedExchangeAddNoFence64((LONG64 volatile *)target, (LONG64)value)
#define __WFQ_SWAP_(target, value) __WFQ_InterlockedExchange64((LONG64 volatile *)target, (LONG64)value)
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
#define __WFQ_SWAP_(target, value) __WFQ_InterlockedExchange((LONG volatile *)target, (LONG)value)
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
*  WFQ FIXED SIZE wait free queue
*
*/
#define _WFQ_UNSET_TAIL_ (size_t) -1

typedef struct {
    size_t volatile count;
    size_t volatile head;
    size_t volatile tail;
    size_t volatile *failed_heads;
    size_t volatile *failed_tails;
    size_t volatile failed_head_sz;
    size_t volatile failed_tail_sz;
    size_t max;
    size_t nconsumer;
    size_t nproducer;
    void **nptr;
} wfqueue_t;

/*
 * max_size - maximum size
 * max_producer - maximum enqueue/produce threads
 * max_consumer - maximum dequeue/consume threads
 *
 */
static wfqueue_t *wfq_create(size_t max_sz, size_t max_producer, size_t max_consumer);
static int wfq_enq(wfqueue_t *q, void* val);
static void* wfq_deq(wfqueue_t *q);
static void wfq_destroy(wfqueue_t *q);

static wfqueue_t *
wfq_create(size_t max_sz, size_t max_producer, size_t max_consumer) {
    size_t i;
    wfqueue_t *q = (wfqueue_t *)malloc(sizeof(wfqueue_t));
    if (!q) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    q->nproducer = max_producer;
    q->nconsumer = max_consumer;
    q->failed_tail_sz = 0;
    q->failed_head_sz = 0;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->nptr = (void**)malloc( max_sz * sizeof(void*));
    q->failed_tails = (size_t*)malloc( q->nconsumer * sizeof(size_t));
    q->failed_heads = (size_t*)malloc( q->nproducer * sizeof(size_t));

    if (!q->nptr && !q->failed_tails) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < max_sz; i++) {
        q->nptr[i] = NULL;
    }
    for (i = 0; i < q->nproducer; i++) {
        q->failed_heads[i] = _WFQ_UNSET_TAIL_;
    }
    for (i = 0; i < q->nconsumer; i++) {
        q->failed_tails[i] = _WFQ_UNSET_TAIL_;
    }

    q->max = max_sz;
    return q;
}

static inline void wfq_enq_must(wfqueue_t *q, void* val) {
    while (!wfq_enq(q, val))
        ;
}

static inline void *wfq_deq_must(wfqueue_t *q) {
    void *_v;
    while (!(_v = wfq_deq(q)))
        ;
    return _v;
}

static int
wfq_enq(wfqueue_t *q, void* val) {
    size_t head, max, __ind, nproducer;
    max = q->max;
    nproducer = q->nproducer;

    __WFQ_SYNC_MEMORY_();
    for (__ind = 0; __WFQ_FETCH_ADD_(&q->failed_head_sz, 0) && __ind < nproducer; __ind++) {
        if ( (head = __WFQ_SWAP_(q->failed_heads + __ind, _WFQ_UNSET_TAIL_) ) != _WFQ_UNSET_TAIL_ ) {
            __WFQ_FETCH_ADD_(&q->failed_head_sz, -1);
            goto ENQ;
        }
    }

    if (__ind == nproducer && __WFQ_FETCH_ADD_(&q->failed_head_sz, 0)) {
        // Retry too busy threads
        return 0;
    } else {
        head = __WFQ_FETCH_ADD_(&q->head, 1);
    }

    if ( __WFQ_FETCH_ADD_(&q->count, 1) < max) {
ENQ:
        if (__WFQ_CAS_(q->nptr + (head % max), NULL, val)) {
            return 1;
        }
    }

    __WFQ_FETCH_ADD_(&q->failed_head_sz, 1);

    for (__ind = 0; __ind < nproducer; __ind++) {
        if (__WFQ_CAS_(q->failed_heads + __ind, _WFQ_UNSET_TAIL_, head)) {
            return 0;
        }
    }

    assert(__ind != nproducer);

    return 0;
}

static void*
wfq_deq(wfqueue_t *q) {
    size_t tail, max, __ind, nconsumer;
    void *val;
    max = q->max;
    nconsumer = q->nconsumer;

    __WFQ_SYNC_MEMORY_();
    for (__ind = 0; __WFQ_FETCH_ADD_(&q->failed_tail_sz, 0) && __ind < nconsumer; __ind++) {
        if ( (tail = __WFQ_SWAP_(q->failed_tails + __ind, _WFQ_UNSET_TAIL_) ) != _WFQ_UNSET_TAIL_ ) {
            __WFQ_FETCH_ADD_(&q->failed_tail_sz, -1);
            goto DEQ;
        }
    }

    if (__ind == nconsumer && __WFQ_FETCH_ADD_(&q->failed_tail_sz, 0)) {
        // Retry too busy threads
        return NULL;
    } else {
        tail = __WFQ_FETCH_ADD_(&q->tail, 1);
    }
DEQ:
    if ( tail < __WFQ_FETCH_ADD_(&q->head, 0) ) {
#if defined __GNUC__ || defined __APPLE__
		if ( (val = __WFQ_SWAP_(q->nptr + (tail % max), NULL) ) ) {
#else
        if ( (val = (void*) __WFQ_SWAP_(q->nptr + (tail % max), NULL) ) ) {
#endif
            __WFQ_FETCH_ADD_(&q->count, -1);
            return val;
        }
    }

    __WFQ_FETCH_ADD_(&q->failed_tail_sz, 1);
    for (__ind = 0; __ind < nconsumer; __ind++) {
        if (__WFQ_CAS_(q->failed_tails + __ind, _WFQ_UNSET_TAIL_, tail)) {
            return NULL;
        }
    }
    assert(__ind != nconsumer);

    return NULL;
}

static void
wfq_destroy(wfqueue_t *q) {
    free((void*) q->failed_heads);
    free((void*) q->failed_tails);
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



// For C++
#ifdef __cplusplus

#include <iostream>
#include <cstring>

namespace tWaitFree {

template <class T>
class Queue {
private:
    wfqueue_t *q;

public:
    Queue(size_t sz, size_t nProducer, size_t nConsumer) {
        q = wfq_create(sz, nProducer, nConsumer);
    }

    inline bool enq(T *v) {
        return wfq_enq(q, (void*)v);
    }

    inline void enqMust(T *v) {
        wfq_enq_must(q, (void*)v);
    }

    inline T* deq() {
        return (T*) wfq_deq(q);
    }

    inline T* deqMust() {
        return (T*) wfq_deq_must(q);
    }

    bool empty() const {
        return wfq_size(q) == 0;
    }

    ~Queue() {
        wfq_destroy(q);
    }
};

}

#endif
// End for C++


#endif
