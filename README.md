<p align="left"><img src="wfqueue_logo.png" alt="wfqueue logo" /></p>

# wfqueue.h [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

c/c++ FIFO wait-free queue, easy built cross platform(no extra dependencies needed) 

Guarantee thread safety memory management, and it's all in one header only, as fast as never wait.


# All Platform tests

GCC/CLANG/G++/CLANG++ | [![Build Status](https://travis-ci.org/Taymindis/wfqueue.svg?branch=master)](https://travis-ci.org/Taymindis/wfqueue)

VS x64/x86 | [![Build status](https://ci.appveyor.com/api/projects/status/k8rwm0cyfd4tq481?svg=true)](https://ci.appveyor.com/project/Taymindis/wfqueue)


#### support MPMC, MPSC, MCSP

## API 
```c

wfqueue_t *wfq_create(size_t size, size_t nexpand);
int wfq_enq(wfqueue_t *q, void* val);
void* wfq_deq(wfqueue_t *q);
void wfq_destroy(wfqueue_t *q);
size_t wfq_size(wfqueue_t *q);
size_t wfq_capacity(wfqueue_t *q);

```


## Example

### if c++

```c++

#define WFQ_EXPANDABLE 1 // comment it if you only want fixed size
#include "wfqueue.h"

// total will be PER_ALLOC_SIZE * NUMBER_OF_EXPAND, expand only use
wfqueue_t *q = wfq_create(PRE_ALLOC_SIZE, NUMBER_OF_EXPAND); 

// wrap in to thread
wfq_enq(q, new ClassVal); // or malloc if c programming, return 1 if success enqueue

// wrap in to thread
ClassVal *s = (intV*)wfq_deq(q); // return NULL/nullptr if no val consuming

if(s) {
  s->do_op();

  delete s;
}


wfq_destroy(q);

```

### if c

```c

#define WFQ_EXPANDABLE 1 // comment it if you only want fixed size
#include "wfqueue.h"

// total will be PER_ALLOC_SIZE * NUMBER_OF_EXPAND, expand only use
wfqueue_t *q = wfq_create(PRE_ALLOC_SIZE, NUMBER_OF_EXPAND); 

// wrap in to thread
wfq_enq(q, malloc(sizeof(ClassVal)); // or malloc if c programming, return 1 if success enqueue

// wrap in to thread
ClassVal *s = (intV*)wfq_deq(q); // return NULL/nullptr if no val consuming

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
	MyQueue ( size_t sz, size_t number_of_expand ) {
		q = wfq_create(sz, number_of_expand);
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

[lfqueue](https://github.com/Taymindis/wfqueue)
