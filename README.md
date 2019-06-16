<p align="left"><img src="wfqueue_logo.png" alt="wfqueue logo" /></p>

# wfqueue.h [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

c/c++ FIFO wait-free queue, easy built cross platform(no extra dependencies needed) 

Guarantee thread safety memory management, and it's all in one header only, as fast as never wait.


# All Platform tests

GCC/CLANG/G++/CLANG++ | [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

VS x64/x86 | [![Build status](https://ci.appveyor.com/api/projects/status/k8rwm0cyfd4tq481?svg=true)](https://ci.appveyor.com/project/Taymindis/wfqueue)

#### support MPMC, MPSC and MCSP

## API 
```c

/*
 * max_size - maximum size
 * max_producer - maximum enqueue/produce threads 
 * max_consumer - maximum dequeue/consume threads 
 *
 */
wfqueue_t *wfq_create(size_t max_sz, size_t max_producer, size_t max_consumer);
int wfq_enq(wfqueue_t *q, void* val);
void* wfq_deq(wfqueue_t *q);
void wfq_destroy(wfqueue_t *q);
size_t wfq_size(wfqueue_t *q);
size_t wfq_capacity(wfqueue_t *q);

// Guarantee enqueue 
void wfq_enq_must(wfqueue_t *q, void* val);
// Guarantee Dequeue
void *wfq_deq_must(wfqueue_t *q);

```


## Example

### if c++

```c++

#include "wfqueue.h"


tWaitFree::Queue<MyVal> myqueue(sz, n_producer, n_consumer);

// wrap in to thread
new ClassVal *s = new ClassVal;


// please use enqMust to guarantee enqueue.
if(myqueue.enq(s)) {
	printf("%s\n", "Done");
} else {
	printf("%s\n", "queue is full, please try again to re-enqueue ");
}

// wrap in to thread
// please use deqMust to guarantee dequeue.
s = (ClassVal*) myqueue.deq() ; // return NULL/nullptr if no val consuming

if(s) {
  s->do_op();

  delete s;
}


```

### if c

```c

#include "wfqueue.h"

// Fixed size of queue
wfqueue_t *q = wfq_create(sz, n_producer, n_consumer); 

// wrap in to thread
ClassVal *s = malloc(sizeof(ClassVal);

// wfq_enq_must for guarantee enqueue
wfq_enq(q, s);

// wrap in to thread
// wfq_deq_must for guarantee dequeue
s = (ClassVal*)wfq_deq(q); // return NULL if no val consuming

if(s) {
  s->do_op();

  free(s);
}

wfq_destroy(q);

```

## Build

include header file in your project

## Next feature target

No pre allocated size needed, it will expand itself (expandable size)

## You may also like lock free queue FIFO

[lfqueue](https://github.com/Taymindis/lfqueue)
