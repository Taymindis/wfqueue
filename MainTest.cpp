/*
 * This is using wfqueue C ++ class
 * compile : g++ -std=c++11 -I./ OverallTest.cpp -pthread -Wall -o overalltest
 * execute
 * valgrind --fair-sched=yes ./overalltest
 */
#include <iostream>
#include <assert.h>
#include <thread>
#include "wfqueue.h"
static const int MILLION = 1000000;
static const int TEST_MAX_INPUT = MILLION;

struct MyVal{
    size_t v;

    MyVal(){}

    MyVal(size_t value) {
        v = value;
    }
};

int TEST_COUNT = 0;

int running_wfq_test(size_t n_producer, size_t n_consumer, const size_t total_threads, const char * test_type) {
    size_t i = 0;
    time_t start_t, end_t;
    double diff_t;
    size_t totalProducing = 0, totalConsuming = 0;
    
    assert((total_threads >= (n_producer + n_consumer)) && "not enough thread to test");
    
    std::thread *testThreads = new std::thread[total_threads];
    
    time(&start_t);
    
    tWaitFree::Queue<MyVal> queue(TEST_MAX_INPUT, n_producer, n_consumer);
    char *testname = (char*)"Fixed size wfqueue test";
    
    for (i = 0; i < n_producer ; i++) {
        testThreads[i] = std::thread([&](int id) {
            int z;
            for (z = 0; z < TEST_MAX_INPUT; z++) {
                MyVal *s = new MyVal(__WFQ_FETCH_ADD_(&totalProducing, 1));
                
                while(!queue.enq(s));
                // wfq_sleep(1);
                // if (xx % 100000 == 0)
                //     printf("%zu\n", xx);
            }
        }, i);
    }
    for (; i < total_threads ; i++) {
        testThreads[i] = std::thread([&](int id) {
            for (;;) {
                MyVal *s;
                while ( ( s = queue.deq() ) ) {
                    // if (s->v % 100000 == 0) {
                    //     printf("t %zu\n", s->v);
                    // }
                    __WFQ_FETCH_ADD_(&totalConsuming, 1);
                    delete s;
                }
                if (__WFQ_FETCH_ADD_(&totalConsuming, 0) >= TEST_MAX_INPUT * (n_producer)) {
                    break;
                }
            }
        }, i);
    }
    
    while (__WFQ_FETCH_ADD_(&totalConsuming, 0) < TEST_MAX_INPUT * (n_producer)) {
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
    printf("======Total Producing = %zu\n", totalConsuming);
    printf("Execution time = %f\n", diff_t);
    assert( queue.empty() && " still left over queue inside ");
    // assert(__WFQ_FETCH_ADD_(&q->head, 0) == __WFQ_FETCH_ADD_(&q->tail, 0) && " head and tail are in incorrect position ");
    
    // assert(q->nenq == 0 && q->ndeq == 0 && " enq deq is in still processing? ");
    
    return 0;
}

int main(void) {
    int ret=0, i;
    
    unsigned int n = std::thread::hardware_concurrency();
    std::cout << n << " Concurrent threads supported \n";
    
    
    if (n > 1) {
        int NUM_PRODUCER = n;
        int NUM_CONSUMER = n;
        int running_set = 50;
        
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, NUM_PRODUCER + NUM_CONSUMER, "MPMC");
        }
        
        NUM_PRODUCER = n;
        NUM_CONSUMER = 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, NUM_PRODUCER + NUM_CONSUMER, "MPSC");
        }
        
        NUM_PRODUCER = 1;
        NUM_CONSUMER = n;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, NUM_PRODUCER + NUM_CONSUMER, "MCSP");
        }
    } else {
        ret = -1;
        printf("One thread is not enough for testing\n");
    }
    
    return ret;
}

