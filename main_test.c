/*
 * compile : g++ -std=c++11 -I./ OverallTest.cpp -pthread -Wall -o overalltest
 * execute
 * valgrind --fair-sched=yes ./overalltest
 */
// #include <iostream>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
// #include <thread>
// #include "wfqueue-test-set.h"
#include "wfqueue.h"
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#define MILLION  1000000
#define TEST_MAX_INPUT  MILLION

typedef struct {
    int v;
} MyVal;

typedef struct {
    size_t nProducer;
    size_t nConsumer;
    size_t nProducing;
    size_t nConsuming;
    wfqueue_t *q;
} wfq_test_config_t;

int TEST_COUNT = 0;
volatile double avg_time = 0;

MyVal* newval(size_t digit) {
    MyVal *data = (MyVal*)malloc(sizeof(MyVal));
    data->v = digit;
    return  data;
}


static void * producing_fn(void *v) {
    int z;
    wfq_test_config_t* config = (wfq_test_config_t*)v;
    wfqueue_t *q = config->q;
    wfq_enq_ctx_t ctx = wfq_init_enq_ctx();

    for (z = 0; z < TEST_MAX_INPUT; z++) {
        MyVal* s = newval(__sync_fetch_and_add(&config->nProducing, 1));

        wfq_enq_must(q, s, &ctx);
        // wfq_single_enq_must(q, s);
        // wfq_sleep(1);
        // if (xx % 100000 == 0)
        //     printf("%zu\n", xx);
    }

    return NULL;
}

static void * consuming_fn(void *v) {
    wfq_test_config_t* config = (wfq_test_config_t*)v;
    wfqueue_t *q = config->q;
    wfq_deq_ctx_t ctx = wfq_init_deq_ctx();

    for (;;) {
        MyVal* s;
        while ( (s = (MyVal*)wfq_deq(q, &ctx) )  ) {
            // if (s->v % 100000 == 0) {
            //     printf("t %zu\n", s->v);
            // }
            free(s);
            __sync_fetch_and_add(&config->nConsuming, 1);
        }
        if (__sync_fetch_and_add(&config->nConsuming, 0) >= TEST_MAX_INPUT * (config->nProducer)) {
            break;
        }
    }
    return NULL;
}

int running_wfq_test(size_t arg_producer, size_t arg_consumer, size_t arg_producing, size_t arg_consuming, const size_t total_threads, const char * test_type) {

    size_t i = 0;
    struct timeval start_t, end_t;
    double diff_t;
    wfq_test_config_t config;

    assert ((total_threads >= (arg_producer + arg_consumer)) && "not enough thread to test");

    pthread_t testThreads[total_threads];

    config.nProducer = arg_producer;
    config.nProducing = arg_producing;
    config.nConsumer = arg_consumer;
    config.nConsuming = arg_consuming;
    config.q = wfq_create(TEST_MAX_INPUT * 8);

    // char *testname = (char*)"Fixed size wfqueue test";

    gettimeofday(&start_t, NULL);


    for (i = 0; i < arg_producer ; i++) {
        pthread_create(testThreads + i, 0, &producing_fn,  &config);
    }
    for (; i < total_threads ; i++) {
        pthread_create(testThreads + i, 0, &consuming_fn,  &config);
    }

    // while (__sync_fetch_and_add(&config.nConsuming, 0) < TEST_MAX_INPUT * (config.nProducer)) {
    //     struct timeval curr;
    //     gettimeofday(&curr, NULL);
    //     if ((curr.tv_usec - start_t.tv_usec) >= (120 * 1000 * 1000)) { // 2 minute
    //         assert(0 && " too long to consuming the queue ");
    //     }
    // }

    for (i = 0; i < total_threads; i++) {
        void *ret = NULL;
        pthread_join(testThreads[i], &ret);
        // free(ret);
    }

    gettimeofday(&end_t, NULL);
    diff_t = (double)(end_t.tv_usec - start_t.tv_usec) / 1000000 + (double)(end_t.tv_sec - start_t.tv_sec);
    // printf("===END Test= %d - %s, test type %s ===\n", ++TEST_COUNT, testname, test_type);
    // printf("======Total consuming = %zu\n", __sync_fetch_and_add(&config.nConsuming, 0));
    // printf("======Left over = %zu\n", wfq_size(config.q));
    // printf("======head=%zu, tail=%zu\n", __atomic_load_n(&config.q->head, __ATOMIC_ACQUIRE), __atomic_load_n(&config.q->tail, __ATOMIC_ACQUIRE));
    // printf("Execution time = %f\n", diff_t);
    assert(wfq_size(config.q) == 0 && " still left over queue inside ");
    assert(__atomic_load_n(&config.q->head, __ATOMIC_ACQUIRE) + 100 > __atomic_load_n(&config.q->tail, __ATOMIC_ACQUIRE) && " head tail gap too far");


    wfq_destroy(config.q);

    // __sync_fetch_and_add(&avg_time, diff_t);
    avg_time += diff_t;

    return 0;
}

int main(void) {
    int ret = 0, i;

    unsigned int n = sysconf(_SC_NPROCESSORS_ONLN); // Linux / MAC OS
    if ( n <= 2) {
        n = 4;
    }

    if (n > 1) {
        int NUM_PRODUCER = n / 2;
        int NUM_CONSUMER = n / 2;
        int running_set = 10;

        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MPMC");
        }

        printf("average time is %.6f\n", avg_time / running_set);
        avg_time = 0;
        usleep(1);

        NUM_PRODUCER = n - 1;
        NUM_CONSUMER = 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MPSC");
        }
        printf("average time is %.6f\n", avg_time / running_set);
        avg_time = 0;
        usleep(1);


        NUM_PRODUCER = 1;
        NUM_CONSUMER =  n - 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MCSP");
        }
        printf("average time is %.6f\n", avg_time / running_set);

    } else {
        ret = -1;
        printf("One thread is not enough for testing\n");
    }

    return ret;
}
