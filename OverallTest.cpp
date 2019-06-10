/*
 * compile : g++ -DWFQ_EXPANDABLE=1 -std=c++11 -I./ OverallTest.cpp -pthread -Wall -o overalltest
 * execute
 * valgrind --fair-sched=yes ./overalltest
 */
#include <iostream>
#include <assert.h>
#include <thread>
#include "wfqueue.h"
static const int MILLION = 1000000;
static const int TEST_MAX_INPUT = MILLION;

typedef struct {
    size_t v;
} MyVal;

int TEST_COUNT = 0;


MyVal* newval(size_t digit) {
    MyVal *data = (MyVal*)malloc(sizeof(MyVal));
    data->v = digit;
    return  data;
}

int running_wfq_test(size_t n_producer, size_t n_consumer, size_t n_producing, size_t n_cosuming, const size_t total_threads, const char * test_type) {
    size_t i = 0;
    time_t start_t, end_t;
    double diff_t;

    assert ((total_threads >= (n_producer + n_consumer)) && "not enough thread to test");

    std::thread *testThreads = new std::thread[total_threads];

    time(&start_t);


    wfqueue_t *q = wfq_create(TEST_MAX_INPUT);
    char *testname = (char*)"Fixed size wfqueue test";

    for (i = 0; i < n_producer ; i++) {
        testThreads[i] = std::thread([&](size_t id) {
            int z;
            for (z = 0; z < TEST_MAX_INPUT; z++) {
                MyVal* s = newval(__WFQ_FETCH_ADD_(&n_producing, 1));

                while(!wfq_enq(q,s));
                // wfq_sleep(1);
                // if (xx % 100000 == 0)
                //     printf("%zu\n", xx);
            }
        }, i);
    }
    for (; i < total_threads ; i++) {
        testThreads[i] = std::thread([&](size_t id) {
            for (;;) {
                MyVal* s;
                while ( (s = (MyVal*)wfq_deq(q) )  ) {
                    if (s->v % 100000 == 0) {
                        printf("t %zu\n", s->v);
                    }
                    free(s);
                    __WFQ_FETCH_ADD_(&n_cosuming, 1);
                }
                if (__WFQ_FETCH_ADD_(&n_cosuming, 0) >= TEST_MAX_INPUT * (n_producer)) {
                    break;
                }
            }
        }, i);
    }

    while (__WFQ_FETCH_ADD_(&n_cosuming, 0) < TEST_MAX_INPUT * (n_producer)) {
        time(&end_t);
        if (difftime(end_t, start_t) >= 120) { // 2 minute
            assert(0 && " too long to consuming the queue");
        }
    }


    for(i = 0 ; i< total_threads ; i++) {
        testThreads[i].join();
    }


    time(&end_t);

    delete []testThreads;


    diff_t = difftime(end_t, start_t);

    printf("===Test= %d - %s, test type %s ===\n", ++TEST_COUNT, testname, test_type);
    printf("======Total Producing = %zu\n", n_cosuming);
    printf("======Total head = %zu\n", q->head);
    printf("Execution time = %f\n", diff_t);
    assert(wfq_size(q) == 0 && " still left over queue inside ");
    // assert(__WFQ_FETCH_ADD_(&q->head, 0) == __WFQ_FETCH_ADD_(&q->tail, 0) && " head and tail are in incorrect position ");
    
    // assert(q->nenq == 0 && q->ndeq == 0 && " enq deq is in still processing? ");

    wfq_destroy(q);
    return 0;
}

int main(int argc, char* argv[]) {
    int ret=0, i;

    unsigned int n = std::thread::hardware_concurrency();
    std::cout << n << " Concurrent threads supported \n";


    if (n > 1) {
        int NUM_PRODUCER = n;
        int NUM_CONSUMER = n;
        int running_set = 5;

        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MPMC");
        }

        NUM_PRODUCER = n;
        NUM_CONSUMER = 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MPSC");
        }

        NUM_PRODUCER = 1;
        NUM_CONSUMER = n;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, 0, 0, NUM_PRODUCER + NUM_CONSUMER, "MCSP");
        }
    } else {
        ret = -1;
        printf("One thread is not enough for testing\n");
    }

    return ret;
}
