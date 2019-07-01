<p align="left"><img src="wfqueue_logo.png" alt="wfqueue logo" /></p>

# wfqueue.h [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

c/c++ FIFO wait-free queue, easy built cross platform(no extra dependencies needed) 

Guarantee thread safety memory management, and it's all in one header only, as fast as never wait.


# All Platform tests

GCC/CLANG/G++/CLANG++ | [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

VS x64/x86 | [![Build status](https://ci.appveyor.com/api/projects/status/k8rwm0cyfd4tq481?svg=true)](https://ci.appveyor.com/project/Taymindis/wfqueue)

#### support MPMC, MPSC and MCSP

## Example

### if c++

```c++

#include "wfqueue.h"


tWaitFree::Queue<MyVal> myqueue(sz);

// wrap in to thread
new ClassVal s(...);
tWaitFree::WfqEnqCtx<MyVal> enqCtx; // init 1 time in a thread only
tWaitFree::WfqDeqCtx<MyVal> deqCtx; // init 1 time in a thread only


// wrap in to thread
// please use enq to guarantee enqueue.
if(myqueue.tryEnq(s, enqCtx)) {
	printf("%s\n", "Enq Done");
} else {
	printf("%s\n", "queue is full, please try again to re-enqueue ");
}

// wrap in to thread
// please use deq to guarantee dequeue.
if(myqueue.tryDeq(s, deqCtx) {
  s->do_op();
}


```

### if c

```c

#include "wfqueue.h"

// Fixed size of queue
wfqueue_t *q = wfq_create(sz); 
wfq_enq_ctx_t enq_ctx = wfq_init_enq_ctx(); // init 1 time in a thread only
wfq_deq_ctx_t deq_ctx = wfq_init_deq_ctx(); // init 1 time in a thread only
// wrap in to thread
ClassVal *s = malloc(sizeof(ClassVal);

// wfq_enq_must for guarantee enqueue
wfq_enq_must(q, s, &enq_ctx);

// wrap in to thread
// wfq_deq_must for guarantee dequeue
s = (ClassVal*)wfq_deq(q, &deq_ctx); // return NULL if no val consuming

if(s) {
  s->do_op();

  free(s);
}

wfq_destroy(q);

```

## Build

include header file in your project


## Comparison with MoodyCamel/ConcurrentQueue

#### 4 Concurrent Threads
| Type 	| WFQUEUE (c++) ms	| WFQUEUE (c)ms	| MOODYCAMEL (ms) 	| INPUTS PER THREAD 	|
|------	|---------------	|-------------	|-----------------	|------------------		|
| MPMC 	| 389.0         	| 370         	| 393.9           	| 1,000,000 * 4    		|
| MPSC 	| 494.50        	| 600         	| 182.0           	| 1,000,000 * 4    		|
| MCSP 	| 252.70        	| 290.5       	| 175.7           	| 1,000,000 * 4    		|


#### 8 Concurrent Threads

| Type 	| WFQUEUE (c++) ms	| WFQUEUE (c)ms	| MOODYCAMEL (ms) 	| INPUTS PER THREAD 	|
|------	|---------------	|-------------	|-----------------	|------------------		|
| MPMC 	| 657.20        	| 642.2        	| 517.90           	| 1,000,000 * 8    		|
| MPSC 	| 1223.50        	| 1610         	| 1471.8           	| 1,000,000 * 8    		|
| MCSP 	| 247.80        	| 245.2       	| 253.2           	| 1,000,000 * 8    		|


#### 8 Concurrent Threads with wfqueue(2 capacity only), create a small queue

| Type 	| WFQUEUE (c++) ms	| WFQUEUE (c)ms	| MOODYCAMEL (ms) 	| INPUTS PER THREAD 	|
|------	|---------------	|-------------	|-----------------	|------------------		|
| MPMC 	| 521	        	| 577        	| 517.90           	| 1,000,000 * 8    		|
| MPSC 	| 1718	        	| 1700         	| 1471.8           	| 1,000,000 * 8    		|
| MCSP 	| 403	        	| 360       	| 253.2           	| 1,000,000 * 8    		|


## Next feature target

No pre allocated size needed, it will expand itself (expandable size)

## You may also like lock free queue FIFO

[lfqueue](https://github.com/Taymindis/lfqueue)
