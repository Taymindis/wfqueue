<p align="left"><img src="wfqueue_logo.png" alt="wfqueue logo" /></p>

# wfqueue.h [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

c/c++ FIFO wait-free queue, easy built cross platform(no extra dependencies needed) 

Guarantee thread safety memory management, and it's all in one header only, as fast as never wait.


# All Platform tests

GCC/CLANG/G++/CLANG++ | [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)



#### support MPSC, MCSP only

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


// Fixed size of queue
wfqueue_t *q = wfq_create(fixed_sz); 

// wrap in to thread
new ClassVal *s = new ClassVal;
wfq_enq(q, s); // or malloc if c programming, return 1 if success enqueue

// wrap in to thread
s = (ClassVal*)wfq_deq(q); // return NULL/nullptr if no val consuming

if(s) {
  s->do_op();

  delete s;
}


wfq_destroy(q);

```

### if c

```c

#include "wfqueue.h"

// Fixed size of queue
wfqueue_t *q = wfq_create(fixed_sz); 

// wrap in to thread
ClassVal *s = malloc(sizeof(ClassVal);
wfq_enq(q, s); // or malloc if c programming, return 1 if success enqueue

// wrap in to thread
s = (ClassVal*)wfq_deq(q); // return NULL/nullptr if no val consuming

if(s) {
  s->do_op();

  free(s);
}

wfq_destroy(q);

```

## Build

include header file in your project


## Wrap this in your class

#### example 

```c++


class MyQueue {
	wfqueue_t *q;
public:
	MyQueue ( size_t sz ) {
		q = wfq_create(sz);
	}
	inline int enq(Xclass *val) {
		return wfq_enq(q, val);
	}

	inline Xclass *deq() {
		return (Xclass*)wfq_deq(q);
	}
	inline size_t getSize() {
		return wfq_size(q);
	}
	~MyQueue() {
		wfq_destroy(q);;
	}
}

```


## You may also like lock free queue FIFO

[lfqueue](https://github.com/Taymindis/lfqueue)
