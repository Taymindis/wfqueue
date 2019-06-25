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

#if defined __GNUC__ || defined __APPLE__
#include <stdlib.h>
#if (__GNUC__ >= 4 && __GNUC_MINOR__ > 7) || defined __APPLE__
#define __WFQ_FETCH_ADD_(ptr, val, order) __atomic_fetch_add(ptr, val, order)
#define __WFQ_CAS_(ptr, cmp, val, succ_order, failed_order) __sync_bool_compare_and_swap(ptr, cmp, val)
#define __WFQ_CAS2_(ptr, cmp, val, succ_order, failed_order) __atomic_compare_exchange_n(ptr, cmp, val, 0, succ_order, failed_order)
#define __WFQ_SWAP_(ptr, val, order) __atomic_exchange_n(ptr, val, order)
#define __WFQ_THREAD_ID_ pthread_self
#define __WFQ_SYNC_MEMORY_() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define __WFQ_LOAD_(ptr, order) __atomic_load_n(ptr, order)
#else
#ifndef __ATOMIC_RELAXED
typedef enum {
    __ATOMIC_RELAXED,
    __ATOMIC_CONSUME,
    __ATOMIC_ACQUIRE,
    __ATOMIC_RELEASE,
    __ATOMIC_ACQ_REL,
    __ATOMIC_SEQ_CST
} memory_order;
#endif
#define __WFQ_FETCH_ADD_(ptr, val, order) __sync_fetch_and_add(ptr, val)
#define __WFQ_CAS_(ptr, cmp, val, succ_order, failed_order) __sync_bool_compare_and_swap(ptr, cmp, val)
#define __WFQ_SWAP_(ptr, val, order) __sync_lock_test_and_set(ptr, val)
#define __WFQ_SYNC_MEMORY_ __sync_synchronize
#define __WFQ_LOAD_(ptr, order) __sync_fetch_and_add(ptr, 0)
#endif
#define __WFQ_THREAD_ID_ pthread_self
#else
typedef enum {
    __ATOMIC_RELAXED,
    __ATOMIC_CONSUME,
    __ATOMIC_ACQUIRE,
    __ATOMIC_RELEASE,
    __ATOMIC_ACQ_REL,
    __ATOMIC_SEQ_CST
} memory_order;
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
#define __WFQ_LOAD_(target, order) InterlockedOr64((LONG64 volatile *)target, 0)
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
#define __WFQ_CAS_(x,y,z, succ_order, failed_order)  ___WFQ_CAS_((LONG volatile *)x, (LONG)y, (LONG)z)
#define __WFQ_FETCH_ADD_(target, value, order) (size_t)__WFQ_InterlockedExchangeAddNoFence((LONG volatile *)target, (LONG)value)
#define __WFQ_SWAP_(target, value, order) __WFQ_InterlockedExchange((LONG volatile *)target, (LONG)value)
#define __WFQ_LOAD_(target, order) InterlockedOr((LONG volatile *)target, 0)
#endif
// thread
#include <windows.h>
#define __WFQ_SYNC_MEMORY_ MemoryBarrier
#define __WFQ_THREAD_ID_ GetCurrentThreadId
#endif

// #include <assert.h>
/*
 *
 *  WFQ FIXED SIZE wait free queue
 *
 */
#define _WFQ_UNSET_SZ_ (size_t) -1
#define _WFQ_MAX_TRY_ 128
static volatile unsigned long debug_cyc_count = 0;
#define ADD_DEBUG_CYC_COUNT __sync_fetch_and_add(&debug_cyc_count, 1)


typedef struct {
    // size_t volatile count;
    size_t volatile head;
    size_t volatile tail;
    size_t max;
    void * volatile *nptr;
} wfqueue_t;

typedef struct {
    size_t _qtix;
    unsigned _hasq: 1;
} wfq_enq_ctx_t;

typedef struct {
    size_t _qtix;
    unsigned _hasq: 1;
} wfq_deq_ctx_t;

/*
 * max_size - maximum size
 * max_producer - maximum enqueue/produce threads
 * max_consumer - maximum dequeue/consume threads
 *
 */
static wfqueue_t *wfq_create(size_t max_sz);
static int wfq_enq(wfqueue_t *q, void* val, wfq_enq_ctx_t *context);
static void* wfq_deq(wfqueue_t *q, wfq_deq_ctx_t *context);
static void wfq_destroy(wfqueue_t *q);

static wfqueue_t *
wfq_create(size_t max_sz) {
    size_t i;
    wfqueue_t *q = (wfqueue_t *)malloc(sizeof(wfqueue_t));
    if (!q) {
        // assert(0 && "malloc error, unable to create wfqueue");
        return NULL;
    }
    // q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->nptr = (void**)malloc( max_sz * sizeof(void*));
    for (i = 0; i < max_sz; i++) {
        q->nptr[i] = NULL;
    }

    q->max = max_sz;
    return q;
}

static inline wfq_enq_ctx_t wfq_init_enq_ctx() {
#if defined __GNUC__ || defined __APPLE__
    return (wfq_enq_ctx_t) { ._qtix = 0, ._hasq = 0 };
#else
    return { 0, 0 };
#endif
}

static inline wfq_deq_ctx_t wfq_init_deq_ctx() {
#if defined __GNUC__ || defined __APPLE__
    return (wfq_deq_ctx_t) { ._qtix = 0, ._hasq = 0  };
#else
    return { 0, 0 };
#endif
}

static inline void wfq_enq_must(wfqueue_t *q, void* val, wfq_enq_ctx_t *ctx) {
    while (!wfq_enq(q, val, ctx))
        __WFQ_SYNC_MEMORY_();
}

static inline void *wfq_deq_must(wfqueue_t *q, wfq_deq_ctx_t *ctx) {
    void *_v;
    while (!(_v = wfq_deq(q, ctx)))
        __WFQ_SYNC_MEMORY_();
    return _v;
}

static int
wfq_enq(wfqueue_t *q, void* val, wfq_enq_ctx_t *ctx) {
    // ADD_DEBUG_CYC_COUNT;
    int n;
    size_t head;
    if (ctx->_hasq) {
        for (n = _WFQ_MAX_TRY_; n > 0; n--) {
            if ( !__WFQ_LOAD_(q->nptr + ctx->_qtix, __ATOMIC_CONSUME) &&
                    __WFQ_CAS_(q->nptr + ctx->_qtix, NULL, val, __ATOMIC_RELEASE, __ATOMIC_RELAXED) ) {
                ctx->_hasq = 0;
                __WFQ_SYNC_MEMORY_();
                return 1;
            }
            __WFQ_SYNC_MEMORY_();

        }
        return 0;
    }

    head = __WFQ_FETCH_ADD_(&q->head, 1, __ATOMIC_RELAXED) % q->max;
    for (n = _WFQ_MAX_TRY_; n > 0; n--) {
        if (!__WFQ_LOAD_(q->nptr + head, __ATOMIC_CONSUME) &&
                __WFQ_CAS_(q->nptr + head, NULL, val, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            __WFQ_SYNC_MEMORY_();
            return 1;
        }
        __WFQ_SYNC_MEMORY_();
    }

    ctx->_qtix = head;
    ctx->_hasq = 1;
    return 0;
}

static void*
wfq_deq(wfqueue_t *q, wfq_deq_ctx_t *ctx) {
    // ADD_DEBUG_CYC_COUNT;
    size_t tail;
    void *val;
    int n;

    if (ctx->_hasq) {
        for (n = _WFQ_MAX_TRY_; n > 0; n--) {
#if defined __GNUC__ || defined __APPLE__
            if (  (val = __WFQ_LOAD_(q->nptr + ctx->_qtix, __ATOMIC_CONSUME)) && __WFQ_CAS_(q->nptr + ctx->_qtix, val, NULL, __ATOMIC_RELEASE, __ATOMIC_RELAXED) ) {
#else
            if ( (val = (void*) __WFQ_SWAP_(q->nptr + ctx->_qtix, NULL, __ATOMIC_ACQ_REL) ) ) {
#endif
                ctx->_hasq = 0;
                __WFQ_SYNC_MEMORY_();
                return val;
            }
            __WFQ_SYNC_MEMORY_();
        }
        return NULL;
    }

    tail = __WFQ_FETCH_ADD_(&q->tail, 1, __ATOMIC_RELAXED) % q->max;
    for (n = _WFQ_MAX_TRY_; n > 0; n--) {
#if defined __GNUC__ || defined __APPLE__
        if ( (val = __WFQ_LOAD_(q->nptr + tail, __ATOMIC_CONSUME)) && __WFQ_CAS_(q->nptr + tail, val, NULL, __ATOMIC_RELEASE, __ATOMIC_RELAXED) ) {
#else
        if ( (val = (void*) __WFQ_SWAP_(&q->nptr[tail], NULL, __ATOMIC_ACQ_REL) ) ) {
#endif
            __WFQ_SYNC_MEMORY_();
            return val;
        }
        __WFQ_SYNC_MEMORY_();
    }

    ctx->_qtix = tail;
    ctx->_hasq = 1;
    return NULL;
}

static void
wfq_destroy(wfqueue_t *q) {
    free((void**) q->nptr);
    free(q);
}


static inline size_t
wfq_size(wfqueue_t *q) {
    long n = ((long)(__WFQ_LOAD_(&q->head, __ATOMIC_RELAXED) - __WFQ_LOAD_(&q->tail, __ATOMIC_RELAXED)));
    return n > 0 ? (size_t) n : 0;
}

static inline size_t
wfq_capacity(wfqueue_t *q) {
    return q->max;
}

#ifdef __cplusplus
}
#endif



// For C++
#ifdef __cplusplus

#include <iostream>
#include <cstring>

namespace tWaitFree {

typedef wfq_enq_ctx_t WfqEnqCtx;
typedef wfq_deq_ctx_t WfqDeqCtx;

inline WfqEnqCtx InitEnqCtx() {
#if defined __GNUC__ || defined __APPLE__
    return (WfqEnqCtx) { ._qtix = 0, ._hasq = 0 };
#else
    return { 0, 0 };
#endif
}

inline WfqDeqCtx InitDeqCtx() {
#if defined __GNUC__ || defined __APPLE__
    return (WfqDeqCtx) { ._qtix = 0, ._hasq = 0  };
#else
    return { 0, 0 };
#endif
}

template <class T>
class Queue {
private:
    wfqueue_t *q;
public:
    Queue(size_t sz) {
        q = wfq_create(sz);
    }

    // try to enqueue with allocated memory as argument
    inline bool tryEnq(T *v, WfqEnqCtx &ctx) {
        return wfq_enq(q, (void*)v, &ctx);
    }

    // must have enqueued with allocated memory as argument
    inline void enq(T *v, WfqEnqCtx &ctx) {
        wfq_enq_must(q, (void*)v, &ctx);
    }

    // must have enqueued with Pass-by-reference as argument
    inline void enq(T &v, WfqEnqCtx &ctx) {
        wfq_enq_must(q, (void*)new T(v), &ctx);
    }

    // try to dequeue and return heap memory, and value need to be deleted
    // return nullptr if dequeue unsucessfully
    inline T* tryDeq(WfqDeqCtx &ctx) {
        return (T*) wfq_deq(q, &ctx);
    }

    // try to dequeue with pass by references
    // return false if dequeue unsucessfully
    bool tryDeq(T &v, WfqDeqCtx &ctx) {
        T* valHeap;
        if ( (valHeap = (T*) wfq_deq(q, &ctx)) ) {
            v = std::move(*valHeap);
            delete valHeap;
            return true;
        }
        return false;
    }

    // must have dequeued and return heap memory, value need to delete
    inline T* deq(WfqDeqCtx &ctx) {
        return (T*) wfq_deq_must(q, &ctx);
    }

    // must have dequeued with pass by references
    inline void deq(T &v, WfqDeqCtx &ctx) {
        T* valHeap;
        valHeap = (T*) wfq_deq_must(q, &ctx);
        v = std::move(*valHeap);
        delete valHeap;
    }

    inline size_t size() const {
        return wfq_size(q);
    }

    inline bool empty() const {
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
