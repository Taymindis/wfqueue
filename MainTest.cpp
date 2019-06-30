/*
 * This is using wfqueue C ++ class
 * compile : g++ -std=c++11 -I./ OverallTest.cpp -pthread -Wall -o overalltest
 * execute
 * valgrind --fair-sched=yes ./overalltest
 */
#include <iostream>
#include <atomic>
#include <assert.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include "wfqueue.h"
static const int MILLION = 1000000;
static const int TEST_MAX_INPUT = MILLION;

#define addcount(x, y) std::atomic_fetch_add_explicit(x, y, std::memory_order_relaxed)

struct MyVal {
    int v = 5;
};

double avgTime = 0;
int TEST_COUNT = 0;
std::atomic_int volatile totalProducing;
std::atomic_int volatile totalConsuming;
int running_wfq_test(size_t n_producer, size_t n_consumer, const size_t total_threads, const char * test_type) {
    size_t i = 0;

    totalProducing = ATOMIC_VAR_INIT(0);
    totalConsuming = ATOMIC_VAR_INIT(0);

    std::atomic_thread_fence(std::memory_order_seq_cst);

    assert((total_threads >= (n_producer + n_consumer)) && "not enough thread to test");

    std::thread *testThreads = new std::thread[total_threads];


    tWaitFree::Queue<MyVal> queue(1024);
    char *testname = (char*)"Fixed size wfqueue test";

    auto begin = std::chrono::high_resolution_clock::now();

    for (i = 0; i < n_producer ; i++) {
        testThreads[i] = std::thread([&](int id) {
            int z;
            tWaitFree::WfqEnqCtx<MyVal> enqCtx;
            for (z = 0; z < TEST_MAX_INPUT; z++) {
                MyVal s;
                s.v = addcount(&totalProducing, 1);
//                if (s.v % 100000 == 0) {
//                    printf("t %d\n", s.v);
//                }

                queue.enq(s, enqCtx);
            }
        }, i);
    }
    for (; i < total_threads ; i++) {
        testThreads[i] = std::thread([&](int id) {
            tWaitFree::WfqDeqCtx<MyVal> deqCtx;
            for (;;) {
                MyVal s;
                if ( queue.tryDeq(s, deqCtx) )  {
                    // if (s.v % 100000 == 0) {
                    //     printf("t %d\n", s.v);
                    // }
                    addcount(&totalConsuming, 1);
                    // delete s;
                }
                if (addcount(&totalConsuming, 0) >= TEST_MAX_INPUT * (n_producer)) {
                    break;
                }
            }
        }, i);
    }

    // while (addcount(&totalConsuming, 0) < TEST_MAX_INPUT * (n_producer)) {
    //     if (std::chrono::duration_cast<std::chrono::seconds>((std::chrono::high_resolution_clock::now() - begin)).count() >= (10 * 60)) { // 2 minute
    //         assert(0 && " too long to consuming the queue");
    //     }
    // }


    for (i = 0 ; i < total_threads ; i++) {
        testThreads[i].join();
    }


    auto end = std::chrono::high_resolution_clock::now();

    delete []testThreads;


    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>((end - begin)).count();

    std::cout << "Test= " << ++TEST_COUNT << ", " << testname << ", Test type " << test_type << std::endl;
    std::cout << "======Total consuming = " <<  totalConsuming.load(std::memory_order_relaxed) << std::endl;
    std::cout << "Execution time = " << ms << "ms" << std::endl;
    avgTime += ms;

    assert( queue.empty() && " still left over queue inside ");
    // assert(addcount(&q->head, 0) == addcount(&q->tail, 0) && " head and tail are in incorrect position ");

    // assert(q->nenq == 0 && q->ndeq == 0 && " enq deq is in still processing? ");

    return 0;
}


int main(void) {
    int ret = 0, i;

    unsigned int n = std::thread::hardware_concurrency();
    std::cout << n << " Concurrent threads supported \n";

    if ( n <= 2) {
        n = 4;
    }

    if (n > 1) {
        int NUM_PRODUCER = n / 2;
        int NUM_CONSUMER = n / 2;
        int running_set = 10;

        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, NUM_PRODUCER + NUM_CONSUMER, "MPMC");
        }


        std::cout << "Avg Time of running set " <<  running_set << " is " << std::fixed << std::setprecision(5) << (avgTime / running_set) << "\n";
        avgTime = 0;

        NUM_PRODUCER = n - 1;
        NUM_CONSUMER = 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, NUM_PRODUCER + NUM_CONSUMER, "MPSC");
        }


        std::cout << "Avg Time of running set " <<  running_set << " is " << std::fixed << std::setprecision(5) << (avgTime / running_set) << "\n";
        avgTime = 0;

        NUM_PRODUCER = 1;
        NUM_CONSUMER = n - 1;
        for (i = 0; i < running_set; i++) {
            ret = running_wfq_test(NUM_PRODUCER, NUM_CONSUMER, NUM_PRODUCER + NUM_CONSUMER, "MCSP");
        }


        std::cout << "Avg Time of running set " <<  running_set << " is " << std::fixed << std::setprecision(5) << (avgTime / running_set) << "\n";
        avgTime = 0;
    } else {
        ret = -1;
        printf("One thread is not enough for testing\n");
    }

    return ret;
}
