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
#include <unistd.h>
#include <string.h>
#define __WFQ_FETCH_ADD_ __sync_fetch_and_add
#define __WFQ_CAS_ __sync_bool_compare_and_swap
#define __WFQ_SWAP_ __sync_lock_test_and_set
#define __WFQ_THREAD_ID_ pthread_self
#define __WFQ_SYNC_MEMORY_ __sync_synchronize
#define __WFQ_YIELD_THREAD_() sleep(0)//; sched_yield()
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
#define __WFQ_YIELD_THREAD_() Sleep(0)//; SwitchToThread()
#define __WFQ_THREAD_ID_ GetCurrentThreadId
#endif

#if 0 

/*
*
*  WFQ Expandable SIZE wait free queue is under code revised and it's not for use at the moment, 
*  it is expandable size with x number of expandable time
*  to use it just define WFQ_EXPANDABLE on top of the header
*/

#define WFQ_PENDING_SET_PTR (void **) -1

typedef struct {
    volatile size_t nenq;
    volatile size_t ndeq;
    volatile size_t head;
    volatile size_t tail;
    volatile size_t count;
    size_t max;
    size_t ttl_max;
    size_t sets;
    void ***nptr;
} wfqueue_t;

/*
*
*  argument nexpand is x number of time can be expanded
*
*/
static wfqueue_t *wfq_create(size_t size, size_t nexpand);
static int _wfq_enq_(wfqueue_t *q, size_t headnow, size_t nenq, void* val);
static void* _wfq_deq_(wfqueue_t *q,  size_t ndeq);
static void wfq_destroy(wfqueue_t *q);

static wfqueue_t *
wfq_create(size_t size, size_t nexpand) {
    size_t i;
    wfqueue_t *q = (wfqueue_t *)malloc(sizeof(wfqueue_t));
    if (!q) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    q->nenq = 0;
    q->ndeq = 0;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->nptr = (void***)malloc(nexpand * sizeof(void**));

    for (i = 0; i < nexpand; i++) {
        q->nptr[i] = NULL;
    }

    q->nptr[0] = (void**) malloc(size * sizeof(void*));
    if (!q->nptr) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < size; i++) {
        q->nptr[0][i] = NULL;
    }
    q->max = size;
    q->sets = nexpand;
    q->ttl_max = nexpand * size;
    return q;
}


/*
* add count then enq
*/
static int
_wfq_enq_(wfqueue_t *q, size_t headnow, size_t nenq, void* val) {
    size_t max, set;
    void *old_val;
    int success, spin;

    if (__WFQ_FETCH_ADD_(&q->count, 0) >= (q->ttl_max - nenq)) {
        fprintf(stderr, "ERROR wfqueue, no more extendable, out of expandable range  ");
        __WFQ_FETCH_ADD_(&q->nenq, -1);
        return 0;
    }

    max = q->max;
    set = (( headnow % q->ttl_max) / max);

    if (__WFQ_CAS_(&q->nptr[set], NULL, WFQ_PENDING_SET_PTR)) {
        void **new_ = (void**) malloc(max * sizeof(void*));
        size_t i;
        if (!new_) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        for (i = 0; i < max; i++) {
            new_[i] = NULL;
        }
        __WFQ_CAS_(&q->nptr[set], WFQ_PENDING_SET_PTR, new_);
    }

    for (spin = 1; __WFQ_CAS_(&q->nptr[set], WFQ_PENDING_SET_PTR, WFQ_PENDING_SET_PTR); spin++) {
        if (spin == WFQ_MAX_SPIN) {
            __WFQ_YIELD_THREAD_();
            spin = 0;
        }
    }
    __WFQ_SYNC_MEMORY_();

    //    nenq = __WFQ_FETCH_ADD_(&q->nenq, 1);
    //    if (count < (max - nenq)) {
    //        head = __WFQ_FETCH_ADD_(&q->head, 1);
    __WFQ_FETCH_ADD_(&q->count, 1);
    headnow %= max;
    old_val = __WFQ_SWAP_(&q->nptr[set][headnow], val);
    success = old_val == NULL;
    __WFQ_FETCH_ADD_(&q->nenq, -1);
    return success;
}

static inline int
wfq_enq(wfqueue_t *q, void* val) {
    return _wfq_enq_(q, __WFQ_FETCH_ADD_(&q->head, 1), __WFQ_FETCH_ADD_(&q->nenq, 1), val);
}

/*
* Deq first then sub count
*/
static void*
_wfq_deq_(wfqueue_t *q,  size_t ndeq) {
    size_t max, set;
    void *val;
    int spin = 0;
    size_t tailnow;

    max = q->max;

    __WFQ_SYNC_MEMORY_();
    while ( __WFQ_FETCH_ADD_(&q->count, 0) > ndeq) {
        tailnow = __WFQ_FETCH_ADD_(&q->tail, 0);
        set = (( tailnow % q->ttl_max) / max);
        if (++spin == WFQ_MAX_SPIN) {
            __WFQ_YIELD_THREAD_();
            spin = 0;
        }
        if (__WFQ_CAS_(&q->nptr[set], NULL, NULL)) {
            continue;
        }
        if (__WFQ_CAS_(&q->nptr[set], WFQ_PENDING_SET_PTR, WFQ_PENDING_SET_PTR)) {
            continue;
        }
        tailnow %= max ;
        if ( (val = __WFQ_SWAP_(&q->nptr[set][tailnow], NULL) ) ) {
            __WFQ_FETCH_ADD_(&q->tail, 1);
            __WFQ_FETCH_ADD_(&q->count, -1);
            __WFQ_FETCH_ADD_(&q->ndeq, -1);
            return val;
        }
    }
    __WFQ_FETCH_ADD_(&q->ndeq, -1);
    return NULL;
}

static inline void*
wfq_deq(wfqueue_t *q) {
    return _wfq_deq_(q, __WFQ_FETCH_ADD_(&q->ndeq, 1));
}

static void
wfq_destroy(wfqueue_t *q) {
    size_t i;
    for (i = 0; i < q->sets; i++) {
        if (!__WFQ_CAS_(q->nptr + i, NULL, NULL) &&
                !__WFQ_CAS_(q->nptr + i, WFQ_PENDING_SET_PTR, WFQ_PENDING_SET_PTR)
           ) {
            free(q->nptr[i]);
        }
    }
    free(q->nptr);
    free(q);
}


#else

/*
*
*  WFQ FIXED SIZE wait free queue, it is faster a bit but size are fixed
*
*/

typedef struct {
    volatile size_t nenq;
    volatile size_t ndeq;
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
    q->nenq = 0;
    q->ndeq = 0;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->nptr = (void**)malloc(fixed_size * sizeof(void*));
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
    size_t nenq, head, max;
    void *old_val;
    int success;
    __WFQ_SYNC_MEMORY_();
    nenq = __WFQ_FETCH_ADD_(&q->nenq, 1);
    max = __WFQ_FETCH_ADD_(&q->max, 0);
    if ((__WFQ_FETCH_ADD_(&q->count, 0)) < (max - nenq)) {
        head = __WFQ_FETCH_ADD_(&q->head, 1);
        head %= max;
        old_val = __WFQ_SWAP_(q->nptr + head, val);
        __WFQ_FETCH_ADD_(&q->count, 1);
        success = old_val == NULL;
    }
    else {
        success = 0;
    }
    __WFQ_FETCH_ADD_(&q->nenq, -1);
    return success;// unreached
}

static void*
wfq_deq(wfqueue_t *q) {
    size_t ndeq, tail, max;
    void *val;
    int spin = 0;

    __WFQ_SYNC_MEMORY_();
    ndeq = __WFQ_FETCH_ADD_(&q->ndeq, 1);
    while (__WFQ_FETCH_ADD_(&q->count, 0)  > ndeq) {
        tail = __WFQ_FETCH_ADD_(&q->tail, 0);
        max = __WFQ_FETCH_ADD_(&q->max, 0);
        tail %= max;
        if ( (val = __WFQ_SWAP_(q->nptr + tail, NULL) ) ) {
            __WFQ_FETCH_ADD_(&q->tail, 1);
            __WFQ_FETCH_ADD_(&q->count, -1);
            __WFQ_FETCH_ADD_(&q->ndeq, -1);
            return val;
        }
        if (++spin == WFQ_MAX_SPIN) {
            __WFQ_YIELD_THREAD_();
            spin = 0;
        }
    }
    __WFQ_FETCH_ADD_(&q->ndeq, -1);
    return NULL;
}


static void
wfq_destroy(wfqueue_t *q) {
    free(q->nptr);
    free(q);
}


#endif


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
