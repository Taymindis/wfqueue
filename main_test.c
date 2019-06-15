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
#include "wfqueue.h"
#include <assert.h>

#define MILLION  1000000
#define TEST_MAX_INPUT  MILLION

typedef struct {
    size_t v;
} MyVal;

typedef struct {
    size_t nProducer;
    size_t nConsumer;
    size_t nProducing;
    size_t nConsuming;
    wfqueue_t *q;
} wfq_test_config_t;

int TEST_COUNT = 0;


MyVal* newval(size_t digit) {
    MyVal *data = (MyVal*)malloc(sizeof(MyVal));
    data->v = digit;
    return  data;
}


void * producing_fn(void *v) {
    int z;
    wfq_test_config_t* config = (wfq_test_config_t*)v;
    wfqueue_t *q = config->q;
    for (z = 0; z < TEST_MAX_INPUT; z++) {
        MyVal* s = newval(__WFQ_FETCH_ADD_(&config->nProducing, 1));

        while (!wfq_enq(q, s));
        // wfq_sleep(1);
        // if (xx % 100000 == 0)
        //     printf("%zu\n", xx);
    }
    return NULL;
}

void * consuming_fn(void *v) {
    wfq_test_config_t* config = (wfq_test_config_t*)v;
    wfqueue_t *q = config->q;
    for (;;) {
        MyVal* s;
        while ( (s = (MyVal*)wfq_deq(q) )  ) {
            // if (s->v % 100000 == 0) {
            //     printf("t %zu\n", s->v);
            // }
            free(s);
            __WFQ_FETCH_ADD_(&config->nConsuming, 1);
        }
        if (__WFQ_FETCH_ADD_(&config->nConsuming, 0) >= TEST_MAX_INPUT * (config->nProducer)) {
            break;
        }
    }
    return NULL;
}

int running_wfq_test(size_t arg_producer, size_t arg_consumer, size_t arg_producing, size_t arg_consuming, const size_t total_threads, const char * test_type) {

    size_t i = 0;
    time_t start_t, end_t;
    double diff_t;
    wfq_test_config_t config;

    assert ((total_threads >= (arg_producer + arg_consumer)) && "not enough thread to test");

    pthread_t testThreads[total_threads];

    time(&start_t);

    config.nProducer = arg_producer;
    config.nProducing = arg_producing;
    config.nConsumer = arg_consumer;
    config.nConsuming = arg_consuming;
    config.q = wfq_create(TEST_MAX_INPUT, arg_producer, arg_consumer);


    char *testname = (char*)"Fixed size wfqueue test";

    for (i = 0; i < arg_producer ; i++) {
        pthread_create(testThreads + i, 0, producing_fn,  &config);
    }
    for (; i < total_threads ; i++) {
        pthread_create(testThreads + i, 0, consuming_fn,  &config);
    }

    while (__WFQ_FETCH_ADD_(&config.nConsuming, 0) < TEST_MAX_INPUT * (config.nProducer)) {
        time(&end_t);
        if (difftime(end_t, start_t) >= 120) { // 2 minute
            assert(0 && " too long to consuming the queue ");
        }
    }

    for (i = 0; i < total_threads; i++) {
        void *ret;
        pthread_join(testThreads[i], &ret);
        // free(ret);
    }


    time(&end_t);


    diff_t = difftime(end_t, start_t);

    printf("===END Test= %d - %s, test type %s ===\n", ++TEST_COUNT, testname, test_type);
    printf("======Total consuming = %zu\n", __WFQ_FETCH_ADD_(&config.nConsuming, 0));
    printf("======Left over = %zu\n", wfq_size(config.q));
    printf("Execution time = %f\n", diff_t);
    assert(wfq_size(config.q) == 0 && " still left over queue inside ");


    wfq_destroy(config.q);
    return 0;
}

int main(void) {
    int ret = 0, i;

    unsigned int n = 8;

    if (n > 1) {
        int NUM_PRODUCER = n;
        int NUM_CONSUMER = n;
        int running_set = 50;

        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MPMC");
        }

        NUM_PRODUCER = n - 1;
        NUM_CONSUMER = 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MPSC");
        }

        NUM_PRODUCER = 1;
        NUM_CONSUMER = n - 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MCSP");
        }
    } else {
        ret = -1;
        printf("One thread is not enough for testing\n");
    }

    return ret;
}
