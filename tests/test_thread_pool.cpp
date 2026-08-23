#include <gtest/gtest.h>
#include "thread_pool.hpp"
#include <atomic>

TEST(ThreadPoolTest, ExecutesAllSubmittedTasks) {
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
