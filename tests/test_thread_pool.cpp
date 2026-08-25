#include <gtest/gtest.h>

#include "thread_pool.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

// not meeting this is a failure, not a reason to wait longer
constexpr auto kTimeout = 2s;

// wait for n events with a timeout; replaces sleep
class Latch {
public:
    explicit Latch(int count) : remaining_(count) {}

    void count_down() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --remaining_;
        }
        cv_.notify_all();
    }

    // true = all events fired, false = timed out
    bool wait_for(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return remaining_ <= 0; });
    }

private:
    std::mutex              mutex_;
    std::condition_variable cv_;
    int                     remaining_;
};

// ===========================================================================
// basics
// ===========================================================================

TEST(ThreadPoolBasicTest, ExecutesSingleTask) {
    std::atomic<bool> done{false};
    {
        ThreadPool pool(2);
        EXPECT_TRUE(pool.submit([&done] { done = true; }));
        pool.shutdown();
    }
    EXPECT_TRUE(done.load());
}

TEST(ThreadPoolBasicTest, ExecutesAllSubmittedTasks) {
    constexpr int kTaskCount = 1000;
    std::atomic<int> counter{0};
    {
        ThreadPool pool(4);
        for (int i = 0; i < kTaskCount; ++i) {
            EXPECT_TRUE(pool.submit([&counter] { ++counter; }));
        }
        pool.shutdown();
        EXPECT_EQ(counter.load(), kTaskCount);
    }
}

TEST(ThreadPoolBasicTest, ExecutesTasksSubmittedFromWorker) {
    // catches deadlock on submit from inside a worker.
    // must wait for both tasks: shutting down early would reject the nested one
    std::atomic<int> counter{0};
    Latch            done(2);
    {
        ThreadPool pool(2);
        pool.submit([&] {
            ++counter;
            done.count_down();
            pool.submit([&] {
                ++counter;
                done.count_down();
            });
        });
        EXPECT_TRUE(done.wait_for(kTimeout)) << "nested task never ran";
        pool.shutdown();
    }
    EXPECT_EQ(counter.load(), 2);
}

TEST(ThreadPoolBasicTest, ZeroWorkerCountFallsBackToOne) {
    // zero workers would swallow tasks forever
    std::atomic<bool> done{false};
    {
        ThreadPool pool(0);
        EXPECT_TRUE(pool.submit([&done] { done = true; }));
        pool.shutdown();
    }
    EXPECT_TRUE(done.load()) << "task never ran: no workers";
}

TEST(ThreadPoolBasicTest, EmptyPoolShutsDownCleanly) {
    ThreadPool pool(4);
    pool.shutdown();
    SUCCEED();
}

// ===========================================================================
// concurrency
// ===========================================================================

TEST(ThreadPoolConcurrencyTest, TasksRunOnDifferentThreads) {
    constexpr int kWorkers = 4;
    Latch          latch(kWorkers);
    std::mutex     mutex;
    std::set<std::thread::id> ids;

    {
        ThreadPool pool(kWorkers);
        for (int i = 0; i < kWorkers; ++i) {
            pool.submit([&] {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ids.insert(std::this_thread::get_id());
                }
                latch.count_down();
            });
        }
        ASSERT_TRUE(latch.wait_for(kTimeout)) << "tasks did not finish in time";
        pool.shutdown();
    }

    EXPECT_GT(ids.size(), 1u) << "a single thread ran everything";
}

TEST(ThreadPoolConcurrencyTest, TasksRunInParallel) {
    // each task checks in, then waits for the rest.
    // serial execution => first task never sees the others => timeout
    constexpr int kWorkers = 4;
    Latch          arrived(kWorkers);
    std::atomic<bool> all_met{false};

    {
        ThreadPool pool(kWorkers);
        for (int i = 0; i < kWorkers; ++i) {
            pool.submit([&] {
                arrived.count_down();
                if (arrived.wait_for(kTimeout)) {
                    all_met = true;
                }
            });
        }
        pool.shutdown();
    }

    EXPECT_TRUE(all_met.load()) << "tasks did not overlap";
}

TEST(ThreadPoolConcurrencyTest, ConcurrentSubmitLosesNoTasks) {
    constexpr int kProducers        = 8;
    constexpr int kTasksPerProducer = 250;
    std::atomic<int> counter{0};

    {
        ThreadPool pool(4);

        std::vector<std::thread> producers;
        producers.reserve(kProducers);
        for (int p = 0; p < kProducers; ++p) {
            producers.emplace_back([&pool, &counter] {
                for (int i = 0; i < kTasksPerProducer; ++i) {
                    pool.submit([&counter] { ++counter; });
                }
            });
        }
        for (auto& t : producers) {
            t.join();
        }

        pool.shutdown();
    }

    EXPECT_EQ(counter.load(), kProducers * kTasksPerProducer);
}

// ===========================================================================
// shutdown
// ===========================================================================

TEST(ThreadPoolShutdownTest, DrainsPendingTasksBeforeReturning) {
    constexpr int kTaskCount = 500;
    std::atomic<int> counter{0};

    ThreadPool pool(2);
    for (int i = 0; i < kTaskCount; ++i) {
        pool.submit([&counter] {
            std::this_thread::sleep_for(1ms);  // keep the queue non-empty
            ++counter;
        });
    }
    pool.shutdown();

    // contract: no workers are left running once shutdown returns
    EXPECT_EQ(counter.load(), kTaskCount);
}

TEST(ThreadPoolShutdownTest, SubmitAfterShutdownReturnsFalse) {
    ThreadPool pool(2);
    pool.shutdown();

    std::atomic<bool> executed{false};
    EXPECT_FALSE(pool.submit([&executed] { executed = true; }));
    EXPECT_FALSE(executed.load()) << "task ran on a stopped pool";
}

TEST(ThreadPoolShutdownTest, IsIdempotent) {
    ThreadPool pool(4);
    pool.shutdown();
    pool.shutdown();
    pool.shutdown();
    SUCCEED();
}

TEST(ThreadPoolShutdownTest, ConcurrentShutdownIsSafe) {
    // main join-safety test; only meaningful under TSan
    constexpr int kStoppers = 8;
    ThreadPool    pool(4);

    std::atomic<int> counter{0};
    for (int i = 0; i < 100; ++i) {
        pool.submit([&counter] { ++counter; });
    }

    std::vector<std::thread> stoppers;
    stoppers.reserve(kStoppers);
    for (int i = 0; i < kStoppers; ++i) {
        stoppers.emplace_back([&pool] { pool.shutdown(); });
    }
    for (auto& t : stoppers) {
        t.join();
    }

    SUCCEED();
}

TEST(ThreadPoolShutdownTest, ShutdownFromWorkerDoesNotHang) {
    // a worker cannot join itself: expect a throw, not a hang.
    // checked inside the task because worker_thread swallows exceptions,
    // so EXPECT_THROW around submit would see nothing
    std::atomic<bool> threw{false};
    Latch             finished(1);

    {
        ThreadPool pool(2);
        pool.submit([&] {
            try {
                pool.shutdown();
            } catch (...) {
                threw = true;
            }
            finished.count_down();
        });

        EXPECT_TRUE(finished.wait_for(kTimeout)) << "self-shutdown hung";
        pool.shutdown();
    }

    EXPECT_TRUE(threw.load()) << "shutdown from a worker must throw";
}

TEST(ThreadPoolShutdownTest, DestructorShutsDownWithoutExplicitCall) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(4);
        for (int i = 0; i < 200; ++i) {
            pool.submit([&counter] { ++counter; });
        }
    }  // destructor must wait for workers
    EXPECT_EQ(counter.load(), 200);
}

TEST(ThreadPoolShutdownTest, DestructorAfterExplicitShutdownIsSafe) {
    {
        ThreadPool pool(4);
        pool.submit([] {});
        pool.shutdown();
    }  // second shutdown must not join twice
    SUCCEED();
}

// ===========================================================================
// exceptions in tasks
// ===========================================================================

TEST(ThreadPoolExceptionTest, PoolSurvivesThrowingTasks) {
    constexpr int kTaskCount = 100;
    std::atomic<int> done{0};

    {
        ThreadPool pool(4);
        for (int i = 0; i < kTaskCount; ++i) {
            pool.submit([&done, i] {
                if (i % 10 == 0) {
                    throw std::runtime_error("boom");
                }
                ++done;
            });
        }
        pool.shutdown();
    }

    EXPECT_EQ(done.load(), 90);
}

TEST(ThreadPoolExceptionTest, WorkerSurvivesNonStandardException) {
    // anything can be thrown, not just std::exception
    std::atomic<int> done{0};
    {
        ThreadPool pool(1);
        pool.submit([] { throw 42; });
        pool.submit([&done] { ++done; });
        pool.shutdown();
    }
    EXPECT_EQ(done.load(), 1) << "worker died on a non-standard exception";
}

TEST(ThreadPoolExceptionTest, ThrowingTaskDoesNotReduceWorkerCount) {
    // single worker: if it dies, nothing after the throw runs
    constexpr int kTaskCount = 50;
    std::atomic<int> done{0};

    {
        ThreadPool pool(1);
        pool.submit([] { throw std::runtime_error("boom"); });
        for (int i = 0; i < kTaskCount; ++i) {
            pool.submit([&done] { ++done; });
        }
        pool.shutdown();
    }

    EXPECT_EQ(done.load(), kTaskCount);
}

// ===========================================================================
// deliberate data race; DISABLED_ because it is UB and must not run in CI.
//   ./thread_pool_test --gtest_also_run_disabled_tests \
//                      --gtest_filter=ThreadPoolRaceDemo.*
// built with TSan it must report a data race
// ===========================================================================

TEST(ThreadPoolRaceDemo, DISABLED_UnprotectedCounterRaces) {
    constexpr int kTaskCount = 10000;
    int           counter    = 0;  // deliberately not atomic

    {
        ThreadPool pool(8);
        for (int i = 0; i < kTaskCount; ++i) {
            pool.submit([&counter] { ++counter; });
        }
        pool.shutdown();
    }

    EXPECT_EQ(counter, kTaskCount) << "expected to come out short";
}

// ===========================================================================
// observers; drop this block if the header has no worker_count()/is_running()
// ===========================================================================

TEST(ThreadPoolIntrospectionTest, ReportsWorkerCount) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.worker_count(), 4u);
    pool.shutdown();
}

// TEST(ThreadPoolIntrospectionTest, IsRunningReflectsState) {
//     ThreadPool pool(2);
//     EXPECT_TRUE(pool.is_running());
//     pool.shutdown();
//     EXPECT_FALSE(pool.is_running());
// }

}  // namespace