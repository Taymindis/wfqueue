/*
 * This is using wfqueue C ++ class
 * mkdir build
 * cd build
 * cmake ..
 * make
 */
#include <iostream>
#include <thread>
#include <wfqueue.h>
#include <limits.h>
#include <gtest/gtest.h>

static const int MILLION = 1000000;
static const int TEST_MAX_INPUT = MILLION;
#define addcount(x, y) __WFQ_FETCH_ADD_(x, y, __ATOMIC_RELAXED)

struct MyVal{
    size_t v;
    MyVal(){}
    MyVal(size_t value) {
        v = value;
    }
};

int TEST_COUNT = 0;

void running_wfq_test(size_t n_producer, size_t n_consumer, const size_t total_threads, const char * test_type) {
    size_t i = 0;
    time_t start_t, end_t;
    double diff_t;
    size_t totalProducing = 0, totalConsuming = 0;


    std::thread *testThreads = new std::thread[total_threads];

    time(&start_t);

    tWaitFree::Queue<MyVal> queue(TEST_MAX_INPUT);
    char *testname = (char*)"Fixed size wfqueue test";

    for (i = 0; i < n_producer ; i++) {
        testThreads[i] = std::thread([&](int id) {
            int z;
            tWaitFree::WfqEnqCtx enqCtx = tWaitFree::InitEnqCtx();
            for (z = 0; z < TEST_MAX_INPUT; z++) {
                MyVal s;
                s.v = addcount(&totalProducing, 1);

                queue.enq(s,enqCtx);
                // wfq_sleep(1);
                // if (xx % 100000 == 0)
                //     printf("%zu\n", xx);
            }
        }, i);
    }
    for (; i < total_threads ; i++) {
        testThreads[i] = std::thread([&](int id) {
            tWaitFree::WfqDeqCtx deqCtx = tWaitFree::InitDeqCtx();
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

    while (addcount(&totalConsuming, 0) < TEST_MAX_INPUT * (n_producer)) {
        time(&end_t);
        if (difftime(end_t, start_t) >= 120) { // 2 minute
            ASSERT_TRUE(0) << "too long to consuming the queue \n";
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

TEST_F(WfqueueTest,MPMC){
    int ret=0, i;
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
}

TEST_F(WfqueueTest,MPSC){
    int ret=0, i;
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
}

TEST_F(WfqueueTest,MCSP){
    int ret=0, i;
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
}



int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
