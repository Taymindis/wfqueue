/*
 * This is using wfqueue C ++ class
 * mkdir build
 * cd build
 * cmake ..
 * make
 */
#include <iostream>
#include <thread>
#include <atomic>
#include <wfqueue.h>
#include <limits.h>
#include <gtest/gtest.h>

static const int MILLION = 1000000;
static const int TEST_MAX_INPUT = MILLION;
#define addcount(x, y) std::atomic_fetch_add_explicit(x, y, std::memory_order_relaxed)

struct MyVal {
    size_t v;
    MyVal() {}
    MyVal(size_t value) {
        v = value;
    }
};

double avgTime = 0;
int TEST_COUNT = 0;
std::atomic_int volatile totalProducing;
std::atomic_int volatile totalConsuming;

void running_wfq_test(size_t n_producer, size_t n_consumer, const size_t total_threads, const char * test_type) {
    size_t i = 0;
    totalProducing = ATOMIC_VAR_INIT(0);
    totalConsuming = ATOMIC_VAR_INIT(0);


    std::thread *testThreads = new std::thread[total_threads];


    auto begin = std::chrono::high_resolution_clock::now();

    tWaitFree::Queue<MyVal> queue(TEST_MAX_INPUT);
    char *testname = (char*)"Fixed size wfqueue test";

    for (i = 0; i < n_producer ; i++) {
        testThreads[i] = std::thread([&](int id) {
            int z;
            tWaitFree::WfqEnqCtx<MyVal> enqCtx;
            for (z = 0; z < TEST_MAX_INPUT; z++) {
                MyVal s;
                s.v = addcount(&totalProducing, 1);

                queue.enq(s, enqCtx);
                // wfq_sleep(1);
                // if (xx % 100000 == 0)
                //     printf("%zu\n", xx);
            }
        }, i);
    }
    for (; i < total_threads ; i++) {
        testThreads[i] = std::thread([&](int id) {
            tWaitFree::WfqDeqCtx<MyVal> deqCtx;
            for (;;) {
                MyVal s;
                while ( queue.tryDeq(s, deqCtx) ) {
                    // if (s->v % 100000 == 0) {
                    //     printf("t %zu\n", s->v);
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
    ASSERT_TRUE(queue.empty()) << " still left over queue inside \n";
}


class WfqueueTest : public ::testing::Test {
protected:
    virtual void SetUp() {
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test
        // (right before the destructor).
    }
};

TEST_F(WfqueueTest, MPMC) {
    int ret = 0, i;
    size_t n = std::thread::hardware_concurrency();
    std::cout << n << " Concurrent threads supported \n";
    int running_set = 10;
    size_t nProducer = n;
    size_t nConsumer = n;
    size_t total_threads = nProducer + nConsumer;

    ASSERT_GT(n, 1);
    for (i = 0; i < running_set; i++) {
        running_wfq_test(nProducer, nConsumer, total_threads, "MPMC");
        // EXPECT_EQ(0, running_wfq_test(nProducer, nConsumer, total_threads, "MPMC"));
    }

    std::cout << "Avg Time of running set " <<  running_set << " is " << std::fixed << std::setprecision(5) << avgTime / running_set << "\n";
    avgTime = 0;
}

TEST_F(WfqueueTest, MPSC) {
    int ret = 0, i;
    size_t n = std::thread::hardware_concurrency();
    std::cout << n << " Concurrent threads supported \n";
    int running_set = 10;
    size_t nProducer = n;
    size_t nConsumer = 1;
    size_t total_threads = nProducer + nConsumer;

    ASSERT_GT(n, 1);
    for (i = 0; i < running_set; i++) {
        running_wfq_test(nProducer, nConsumer, total_threads, "MPSC");
        // EXPECT_EQ(0, running_wfq_test(nProducer, nConsumer, total_threads, "MPMC"));
    }

    std::cout << "Avg Time of running set " <<  running_set << " is " << std::fixed << std::setprecision(5) << avgTime / running_set << "\n";
    avgTime = 0;
}

TEST_F(WfqueueTest, MCSP) {
    int ret = 0, i;
    size_t n = std::thread::hardware_concurrency();
    std::cout << n << " Concurrent threads supported \n";
    int running_set = 10;
    size_t nProducer = 1;
    size_t nConsumer = n;
    size_t total_threads = nProducer + nConsumer;

    ASSERT_GT(n, 1);
    for (i = 0; i < running_set; i++) {
        running_wfq_test(nProducer, nConsumer, total_threads, "MCSP");
        // EXPECT_EQ(0, running_wfq_test(nProducer, nConsumer, total_threads, "MPMC"));
    }

    std::cout << "Avg Time of running set " <<  running_set << " is " << std::fixed << std::setprecision(5) << avgTime / running_set << "\n";
    avgTime = 0;
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
