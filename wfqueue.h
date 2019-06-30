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

#ifndef __cplusplus
#if defined __GNUC__ || defined __APPLE__
#include <stdlib.h>
#if ((__GNUC__ >= 4 && __GNUC_MINOR__ > 7) || (__GNUC__ >= 5 )) || defined __APPLE__ || defined __clang__
#define __WFQ_FETCH_ADD_(ptr, val, order) __atomic_fetch_add(ptr, val, order)
#define __WFQ_CAS_(ptr, cmp, val, succ_order, failed_order) __sync_bool_compare_and_swap(ptr, cmp, val, 0, succ_order, failed_order)
#define __WFQ_CAS2_(ptr, cmp, val, succ_order, failed_order) __atomic_compare_exchange_n(ptr, cmp, val, 1, succ_order, failed_order)
#define __WFQ_SWAP_(ptr, val, order) __atomic_exchange_n(ptr, val, order)
#define __WFQ_THREAD_ID_ pthread_self
#define __WFQ_SYNC_MEMORY_() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define __WFQ_LOAD_(ptr, order) __atomic_load_n(ptr, order)
#else
#error "__atomic built in is not available or not supported"
#endif
#endif

// #include <assert.h>
/*
 *
 *  WFQ FIXED SIZE wait free queue
 *
 */
#define _WFQ_UNSET_SZ_ (size_t) -1
#define _WFQ_MAX_TRY_ 128
// static unsigned long debug_cyc_count = 0;
// #define ADD_DEBUG_CYC_COUNT __sync_fetch_and_add(&debug_cyc_count, 1)
#define _WFQ_NULL_ ((void *) 0)
#define _WFQ_CACHE_64_ALIGNED_ __attribute__((aligned(64)))
#define _WFQ_CACHE_128_ALIGNED_ __attribute__((aligned(128)))

typedef struct {
    size_t volatile head _WFQ_CACHE_64_ALIGNED_;
    size_t volatile tail _WFQ_CACHE_64_ALIGNED_;
    size_t max _WFQ_CACHE_64_ALIGNED_;
    void * volatile *nptr _WFQ_CACHE_64_ALIGNED_;
}  wfqueue_t _WFQ_CACHE_128_ALIGNED_;

typedef struct {
    // size_t qtix_ _WFQ_CACHE_64_ALIGNED_;
    void * volatile *_nptrs _WFQ_CACHE_64_ALIGNED_;
    unsigned hasq_: 1;
} wfq_enq_ctx_t _WFQ_CACHE_128_ALIGNED_;

typedef struct {
    // size_t qtix_ _WFQ_CACHE_64_ALIGNED_;
    void * volatile *_nptrs _WFQ_CACHE_64_ALIGNED_;
    unsigned hasq_: 1;
} wfq_deq_ctx_t _WFQ_CACHE_128_ALIGNED_;

/*
 * max_size - maximum size
 */
static wfqueue_t *wfq_create(size_t max_sz);
static int wfq_enq(wfqueue_t *q, void* val, wfq_enq_ctx_t *context);
static int wfq_single_enq(wfqueue_t *q, void* val);
static void* wfq_deq(wfqueue_t *q, wfq_deq_ctx_t *context);
static void* wfq_single_deq(wfqueue_t *q);
static void wfq_destroy(wfqueue_t *q);
static inline void *_wfq_malloc(size_t alignment, size_t size) {
#if ( __clang__ || _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600 )
    void * ptr;
    int ret = posix_memalign(&ptr, alignment, size);
    if (ret != 0) {
        fprintf(stderr, "error posix_memalign");
        abort();
    }
    return ptr;
#elif _ISOC11_SOURCE
    return aligned_alloc(alignment, size);
#else
    return malloc(size);
#endif

}

static wfqueue_t *
wfq_create(size_t max_sz) {
    size_t i;
    wfqueue_t *q = (wfqueue_t *)_wfq_malloc(64, sizeof(wfqueue_t));
    if (!q) {
        // assert(0 && "malloc error, unable to create wfqueue");
        return _WFQ_NULL_;
    }
    // q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->nptr = (void**)_wfq_malloc(64,  max_sz * sizeof(void*));
    for (i = 0; i < max_sz; i++) {
        q->nptr[i] = _WFQ_NULL_;
    }

    q->max = max_sz;
    __WFQ_SYNC_MEMORY_();
    return q;
}

static inline wfq_enq_ctx_t wfq_init_enq_ctx() {
#if defined __GNUC__ || defined __APPLE__
    return (wfq_enq_ctx_t) { ._nptrs = 0, .hasq_ = 0 };
#else
    return { 0, 0 };
#endif
}

static inline wfq_deq_ctx_t wfq_init_deq_ctx() {
#if defined __GNUC__ || defined __APPLE__
    return (wfq_deq_ctx_t) { ._nptrs = 0, .hasq_ = 0 };
#else
    return { 0, 0 };
#endif
}

static inline void wfq_enq_must(wfqueue_t *q, void* val, wfq_enq_ctx_t *ctx) {
    while (!wfq_enq(q, val, ctx))
        __WFQ_SYNC_MEMORY_();
}

static inline void wfq_single_enq_must(wfqueue_t *q, void* val) {
    while (!wfq_single_enq(q, val))
        __WFQ_SYNC_MEMORY_();
}

static inline void *wfq_deq_must(wfqueue_t *q, wfq_deq_ctx_t *ctx) {
    void *_v;
    while (!(_v = wfq_deq(q, ctx)))
        __WFQ_SYNC_MEMORY_();
    return _v;
}

static inline void *wfq_single_deq_must(wfqueue_t *q) {
    void *_v;
    while (!(_v = wfq_single_deq(q)))
        __WFQ_SYNC_MEMORY_();
    return _v;
}

static int
wfq_enq(wfqueue_t *q, void* val, wfq_enq_ctx_t *ctx) {
    // ADD_DEBUG_CYC_COUNT;
    int n;
    size_t head;
    void *currval, * volatile *nptrs;
    if ( ctx->hasq_ ) {
        nptrs = ctx->_nptrs;
        // nptrs = q->nptr + ctx->qtix_;
        currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME);
        for (n = _WFQ_MAX_TRY_; n > 0; n--) {
            // if ( !(currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME) ) &&
            if ( (!currval || !(currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME) ) ) &&
                    __WFQ_CAS2_(nptrs, &currval,  val, __ATOMIC_RELEASE, __ATOMIC_CONSUME) ) {
                ctx->hasq_ = 0;
                __WFQ_SYNC_MEMORY_();
                return 1;
            }
            __WFQ_SYNC_MEMORY_();
        }
        return 0;
    }

    head = __WFQ_FETCH_ADD_(&q->head, 1, __ATOMIC_RELAXED) % q->max;
    nptrs = q->nptr + head;
    currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME);
    for (n = _WFQ_MAX_TRY_; n > 0; n--) {
        // if (  !currval  &&
        // if ( !(currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME) ) &&
        if ( (!currval || !(currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME) ) ) &&
                __WFQ_CAS2_(nptrs, &currval, val, __ATOMIC_RELEASE, __ATOMIC_CONSUME)) {
            __WFQ_SYNC_MEMORY_();
            return 1;
        }
        __WFQ_SYNC_MEMORY_();
    }

    ctx->_nptrs = nptrs;
    ctx->hasq_ = 1;
    // ctx->qtix_ = head;
    return 0;
}

static int
wfq_single_enq(wfqueue_t *q, void* val) {
    size_t head;
    void *currval, * volatile *nptrs;
    head = q->head % q->max;
    nptrs = q->nptr + head;
    currval = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME);
    if (__WFQ_CAS2_(nptrs, &currval, val, __ATOMIC_RELEASE, __ATOMIC_CONSUME)) {
        q->head++;
        return 1;
    }
    __WFQ_SYNC_MEMORY_();
    return 0;
}

static void*
wfq_deq(wfqueue_t *q, wfq_deq_ctx_t *ctx) {
    // ADD_DEBUG_CYC_COUNT;
    size_t tail;
    void *val, * volatile *nptrs;
    int n;

    if ( ctx->hasq_ ) {
        nptrs = ctx->_nptrs;
        val = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME);
        for (n = _WFQ_MAX_TRY_; n > 0; n--) {
            // if (  val  &&
            if ( ( val || (val = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME)) ) &&
                    __WFQ_CAS2_(nptrs, &val, _WFQ_NULL_, __ATOMIC_RELEASE, __ATOMIC_CONSUME) ) {
                ctx->hasq_ = 0;
                __WFQ_SYNC_MEMORY_();
                return val;
            }
            __WFQ_SYNC_MEMORY_();
        }
        return _WFQ_NULL_;
    }

    tail = __WFQ_FETCH_ADD_(&q->tail, 1, __ATOMIC_RELAXED) % q->max;
    nptrs = q->nptr + tail;
    val = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME);
    for (n = _WFQ_MAX_TRY_; n > 0; n--) {
        // if (  val  &&
        if ( ( val || (val = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME)) ) &&
                __WFQ_CAS2_(nptrs, &val, _WFQ_NULL_, __ATOMIC_RELEASE, __ATOMIC_CONSUME) ) {
            __WFQ_SYNC_MEMORY_();
            return val;
        }
        __WFQ_SYNC_MEMORY_();
    }

    // ctx->qtix_ = tail;
    ctx->_nptrs = nptrs;
    ctx->hasq_ = 1;

    return _WFQ_NULL_;
}

static void*
wfq_single_deq(wfqueue_t *q) {
    // ADD_DEBUG_CYC_COUNT;
    size_t tail;
    void *val, * volatile *nptrs;
    int n;

    tail = q->tail % q->max;
    nptrs = q->nptr + tail;
    val = __WFQ_LOAD_(nptrs, __ATOMIC_CONSUME);
    for (n = _WFQ_MAX_TRY_; n > 0; n--) {
        if ( val  &&
                __WFQ_CAS2_(nptrs, &val, _WFQ_NULL_, __ATOMIC_RELEASE, __ATOMIC_CONSUME) ) {
            q->tail++;
            return val;
        }
    }
    return _WFQ_NULL_;
}

static void
wfq_destroy(wfqueue_t *q) {
    free((void**) q->nptr);
    free(q);
}

static inline size_t
wfq_size(wfqueue_t *q) {
    size_t _h = __WFQ_LOAD_(&q->head, __ATOMIC_RELAXED), _t = __WFQ_LOAD_(&q->tail, __ATOMIC_RELAXED);
    return _h > _t ? _h - _t : 0;
}

static inline size_t
wfq_capacity(wfqueue_t *q) {
    return q->max;
}

#endif



// For C++
#ifdef __cplusplus
#if defined _WIN32 || defined _WIN64
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#endif
#include <atomic>
#include <cstring>

#define _WFQ_MAX_TRY_ 128
// static unsigned long debug_cyc_count = 0;
// #define ADD_DEBUG_CYC_COUNT __sync_fetch_and_add(&debug_cyc_count, 1)
#define _WFQ_NULL_ 0
#if defined __GNUC__ || defined __APPLE__
#define _WFQ_ALIGNED_SZ 128
#define _WFQ_CACHE_128_ALIGNED_ __attribute__((aligned(_WFQ_ALIGNED_SZ)))
#define _WFQ_MSVC_CACHE_128_ALIGNED_
#else
#define _WFQ_ALIGNED_SZ 128
#define _WFQ_CACHE_128_ALIGNED_
#define _WFQ_MSVC_CACHE_128_ALIGNED_ __declspec(align(_WFQ_ALIGNED_SZ))
#endif


typedef std::atomic_size_t atomic_wfqindex;
typedef size_t wfqindex;
static const size_t increase_one = 1;

namespace tWaitFree {

template <class eT>
struct _WFQ_MSVC_CACHE_128_ALIGNED_ _WFQ_CACHE_128_ALIGNED_ WfqEnqCtx {
    unsigned hasq_: 1 _WFQ_CACHE_128_ALIGNED_;
    eT *pendingNewVal_ _WFQ_CACHE_128_ALIGNED_;
    std::atomic<eT*> *nptr_ _WFQ_CACHE_128_ALIGNED_;

    WfqEnqCtx() {
        hasq_ = 0;
        pendingNewVal_ = nullptr;
        nptr_ = nullptr;
    }
};

template <class dT>
struct _WFQ_MSVC_CACHE_128_ALIGNED_ _WFQ_CACHE_128_ALIGNED_ WfqDeqCtx {
    unsigned hasq_: 1 _WFQ_CACHE_128_ALIGNED_;
    std::atomic<dT*> *nptr_ _WFQ_CACHE_128_ALIGNED_;

    WfqDeqCtx() {
        hasq_ = 0;
        nptr_ = nullptr;
    }
} ;

template <class T>
class _WFQ_MSVC_CACHE_128_ALIGNED_ _WFQ_CACHE_128_ALIGNED_ Queue {
private:
    atomic_wfqindex head_ _WFQ_CACHE_128_ALIGNED_;
    atomic_wfqindex tail_ _WFQ_CACHE_128_ALIGNED_;
    size_t max_ _WFQ_CACHE_128_ALIGNED_;
    std::atomic<T*> *nptr_ _WFQ_CACHE_128_ALIGNED_;
    void *freebuf_;

public:
    Queue( size_t capacity) {
        // nptr_ = (std::atomic<T*> *) std::malloc( capacity * (sizeof(std::atomic<T*>) + _WFQ_ALIGNED_SZ  ) );
        size_t total_sz =  capacity * (sizeof(std::atomic<T*>) + _WFQ_ALIGNED_SZ  );

        freebuf_ = (void*) std::malloc(total_sz + sizeof(void*));

        void *alloc_buf = (void *)(((uintptr_t) freebuf_) + sizeof(void*));

        nptr_ = reinterpret_cast<std::atomic<T*> *>(std::align(_WFQ_ALIGNED_SZ, capacity * sizeof(std::atomic<T*>), alloc_buf, total_sz));

        for (size_t i = 0; i < capacity; i++) {
            nptr_[i] = ATOMIC_VAR_INIT(nullptr);
        }

        head_ = ATOMIC_VAR_INIT(0);
        tail_ = ATOMIC_VAR_INIT(0);
        max_ = capacity;
        atomic_thread_fence(std::memory_order_seq_cst);
    }

    // try to enqueue
    bool tryEnq(T &v, WfqEnqCtx<T> &ctx) {
        wfqindex head;
        T *currval, *newVal;
        std::atomic<T*> *nptrs;
        if (ctx.hasq_) {
            nptrs = ctx.nptr_;
            newVal = ctx.pendingNewVal_;
            currval = nptrs->load(std::memory_order_consume);
            for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
                // if (!currval &&
                if ( (!currval || !( currval = nptrs->load(std::memory_order_consume) ) ) &&
                        nptrs->compare_exchange_strong(currval, newVal,
                                                       std::memory_order_release,
                                                       std::memory_order_consume)) {
                    ctx.hasq_ = 0;
                    atomic_thread_fence(std::memory_order_seq_cst);
                    return true;
                }
                atomic_thread_fence(std::memory_order_seq_cst);
            }
            return false;
        }
        newVal = new T(v);
        head = std::atomic_fetch_add_explicit(&head_, increase_one, std::memory_order_relaxed) % max_;
        nptrs = &nptr_[head];
        currval = nptrs->load(std::memory_order_consume);
        for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
            // if (!currval &&
            if ( (!currval || !( currval = nptrs->load(std::memory_order_consume) ) ) &&
                    nptrs->compare_exchange_strong(currval, newVal,
                                                   std::memory_order_release,
                                                   std::memory_order_consume)) {
                atomic_thread_fence(std::memory_order_seq_cst);
                return true;
            }
            atomic_thread_fence(std::memory_order_seq_cst);
        }
        ctx.nptr_ = nptrs;
        ctx.pendingNewVal_ = newVal;
        ctx.hasq_ = 1;
        return false;
    }


    bool tryEnq(T *newVal, WfqEnqCtx<T> &ctx) {
        wfqindex head;
        T *currval;
        std::atomic<T*> *nptrs;
        if (ctx.hasq_) {
            nptrs = ctx.nptr_;
            currval = nptrs->load(std::memory_order_consume);
            for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
                // if (!currval &&
                if ( (!currval || !( currval = nptrs->load(std::memory_order_consume) ) ) &&
                        nptrs->compare_exchange_strong(currval, newVal,
                                                       std::memory_order_release,
                                                       std::memory_order_consume)) {
                    ctx.hasq_ = 0;
                    atomic_thread_fence(std::memory_order_seq_cst);
                    return true;
                }
                atomic_thread_fence(std::memory_order_seq_cst);
            }
            return false;
        }
        head = std::atomic_fetch_add_explicit(&head_, increase_one, std::memory_order_relaxed) % max_;
        nptrs = &nptr_[head];
        currval = nptrs->load(std::memory_order_consume);
        for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
            // if (!currval &&
            if ( (!currval || !( currval = nptrs->load(std::memory_order_consume) ) ) &&
                    nptrs->compare_exchange_strong(currval, newVal,
                                                   std::memory_order_release,
                                                   std::memory_order_consume)) {
                atomic_thread_fence(std::memory_order_seq_cst);
                return true;
            }
            atomic_thread_fence(std::memory_order_seq_cst);
        }
        ctx.nptr_ = nptrs;
        ctx.hasq_ = 1;
        return false;
    }

    bool tryDeq(T &v, WfqDeqCtx<T> &ctx) {
        wfqindex tail;
        std::atomic<T*> *nptrs;
        T *val;
        if (ctx.hasq_) {
            nptrs = ctx.nptr_;
            val = nptrs->load(std::memory_order_consume);
            for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
                // if (val &&
                if ( ( val || (val = nptrs->load(std::memory_order_consume) ) ) &&
                        nptrs->compare_exchange_strong(val, nullptr,
                                                       std::memory_order_release,
                                                       std::memory_order_consume)) {
                    ctx.hasq_ = 0;
                    atomic_thread_fence(std::memory_order_seq_cst);
                    v = *val;
                    delete val;
                    return true;
                }
                atomic_thread_fence(std::memory_order_seq_cst);
            }
            return false;
        }

        tail = std::atomic_fetch_add_explicit(&tail_, increase_one, std::memory_order_relaxed) % max_;
        nptrs = &nptr_[tail];
        val = nptrs->load(std::memory_order_consume);
        for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
            // if ( val
            if ( ( val || (val = nptrs->load(std::memory_order_consume) ) ) &&
                    nptrs->compare_exchange_strong(val, nullptr,
                                                   std::memory_order_release,
                                                   std::memory_order_consume) ) {
                atomic_thread_fence(std::memory_order_seq_cst);
                v = *val;
                delete val;
                return true;
            }
            atomic_thread_fence(std::memory_order_seq_cst);
        }

        ctx.nptr_ = nptrs;
        ctx.hasq_ = 1;
        return false;
    }

    T* tryDeq(WfqDeqCtx<T> &ctx) {
        wfqindex tail;
        std::atomic<T*> *nptrs;
        T *val;
        if (ctx.hasq_) {
            nptrs = ctx.nptr_;
            val = nptrs->load(std::memory_order_consume);
            for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
                // if (val &&
                if ( ( val || (val = nptrs->load(std::memory_order_consume) ) ) &&
                        nptrs->compare_exchange_strong(val, nullptr,
                                                       std::memory_order_release,
                                                       std::memory_order_consume)) {
                    ctx.hasq_ = 0;
                    atomic_thread_fence(std::memory_order_seq_cst);
                    return val;
                }
                atomic_thread_fence(std::memory_order_seq_cst);
            }
            return nullptr;
        }

        tail = std::atomic_fetch_add_explicit(&tail_, increase_one, std::memory_order_relaxed) % max_;
        nptrs = &nptr_[tail];
        val = nptrs->load(std::memory_order_consume);
        for (int n = _WFQ_MAX_TRY_; n > 0; n--) {
            // if ( val
            if ( ( val || (val = nptrs->load(std::memory_order_consume) ) ) &&
                    nptrs->compare_exchange_strong(val, nullptr,
                                                   std::memory_order_release,
                                                   std::memory_order_consume) ) {
                atomic_thread_fence(std::memory_order_seq_cst);
                return val;
            }
            atomic_thread_fence(std::memory_order_seq_cst);
        }

        ctx.nptr_ = nptrs;
        ctx.hasq_ = 1;
        return nullptr;
    }

    inline void enq(T &v, WfqEnqCtx<T> &ctx) {
        while (!tryEnq(v, ctx))
            atomic_thread_fence(std::memory_order_seq_cst);
    }
    inline void enq(T *v, WfqEnqCtx<T> &ctx) {
        while (!tryEnq(v, ctx))
            atomic_thread_fence(std::memory_order_seq_cst);
    }

    inline void deq(T &v, WfqDeqCtx<T> &ctx) {
        while (!tryDeq(v, ctx))
            atomic_thread_fence(std::memory_order_seq_cst);
    }

    inline T* deq(WfqDeqCtx<T> &ctx) {
        T *v_;
        while ( !(v_= tryDeq(ctx)) )
            atomic_thread_fence(std::memory_order_seq_cst);
        return v_;
    }

    inline size_t getSize() const {
        size_t _h = head_.load (std::memory_order_relaxed), _t = tail_.load(std::memory_order_relaxed);
        return _h > _t ? _h - _t : 0;
    }

    inline bool empty() const {
        return getSize() == 0;
    }

    ~Queue() noexcept {
        std::free(freebuf_);
    }
};

}

#endif
// End for C++

#endif
