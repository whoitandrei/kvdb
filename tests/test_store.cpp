#include <gtest/gtest.h>
#include <latch>
#include <string>
#include <thread>

#include "store.hpp"

namespace {

TEST(StoreBasicTest, SetAndGet) {
    Store store;
    store.set("key1", "value1");
    store.set("key2", "value2");

    auto value1 = store.get("key1");
    ASSERT_TRUE(value1.has_value());
    EXPECT_EQ(value1.value(), "value1");

    auto value2 = store.get("key2");
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(value2.value(), "value2");

    auto value3 = store.get("nonexistent_key");
    EXPECT_FALSE(value3.has_value());
}

TEST(StoreBasicTest, GetMissingKeyReturnsNullopt) {
    Store store;
    store.set("key1", "value1");

    auto value2 = store.get("key2");
    EXPECT_EQ(value2, std::nullopt);
}

TEST(StoreBasicTest, SetOverwritesExistingValue) {
    Store store;
    store.set("key", "value1");
    store.set("key", "value2");

    auto value = store.get("key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value2");
}

TEST(StoreBasicTest, DelExistingKeyReturnsTrueAndRemoves) {
    Store store;
    store.set("key", "value");

    EXPECT_TRUE(store.del("key"));
    EXPECT_EQ(store.get("key"), std::nullopt);
}

TEST(StoreBasicTest, DelMissingKeyReturnsFalse) {
    Store store;
    EXPECT_FALSE(store.del("nonexistent_key"));
}

TEST(StoreBasicTest, DelThenSetSameKeyWorks) {
    Store store;
    store.set("key", "value1");
    ASSERT_TRUE(store.del("key"));

    store.set("key", "value2");
    auto value = store.get("key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value2");
}

TEST(StoreBasicTest, MultipleKeysIndependent) {
    Store store;
    store.set("key1", "value1");
    store.set("key2", "value2");
    store.set("key3", "value3");

    EXPECT_TRUE(store.del("key2"));

    auto value1 = store.get("key1");
    ASSERT_TRUE(value1.has_value());
    EXPECT_EQ(value1.value(), "value1");

    EXPECT_EQ(store.get("key2"), std::nullopt);

    auto value3 = store.get("key3");
    ASSERT_TRUE(value3.has_value());
    EXPECT_EQ(value3.value(), "value3");
}

TEST(StoreBasicTest, GetReturnsIndependentCopy) {
    Store store;
    store.set("key", "original");

    auto snapshot = store.get("key");
    ASSERT_TRUE(snapshot.has_value());

    store.set("key", "changed");

    EXPECT_EQ(snapshot.value(), "original");
    EXPECT_EQ(store.get("key").value(), "changed");
}

TEST(StoreConcurrencyTest, ConcurrentWritesDifferentKeys) {
    struct Job {
        std::string name;
        std::optional<std::string> product;
        std::thread action;
    };

    Job jobs[]{{"thread1"}, {"thread2"}, {"thread3"}, {"thread4"}};

    std::latch work{std::size(jobs)};
    Store store;

    auto work_thread = [&](Job& job) {
        work.count_down();
        work.wait();
        store.set(job.name, job.name);
        job.product = store.get(job.name);
    };

    for (auto& job : jobs) {
        job.action = std::thread{work_thread, std::ref(job)};
    }

    for (auto& job : jobs) {
        job.action.join();
    }

    for (auto& job : jobs) {
        ASSERT_TRUE(job.product.has_value());
        EXPECT_EQ(job.product.value(), job.name);
    }
}
 
TEST(StoreConcurrencyTest, ConcurrentReadsSameKey) {
    constexpr int kReaderCount = 8;
    Store store;
    store.set("key", "value");
 
    std::latch start{kReaderCount};
    std::atomic<int> mismatch_count{0};
    std::vector<std::thread> readers;
 
    for (int i = 0; i < kReaderCount; ++i) {
        readers.emplace_back([&] {
            start.count_down();
            start.wait();
            auto result = store.get("key");
            if (!result.has_value() || result.value() != "value") {
                mismatch_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : readers) {
        t.join();
    }
 
    EXPECT_EQ(mismatch_count.load(), 0);
}
 
TEST(StoreConcurrencyTest, ConcurrentReadWriteSameKey) {
    constexpr int kIterations = 2000;
    Store store;
    store.set("key", "value_0");
 
    std::atomic<bool> stop{false};
    std::atomic<int> corrupted_reads{0};
 
    std::thread writer([&] {
        for (int i = 1; i <= kIterations; ++i) {
            store.set("key", "value_" + std::to_string(i));
        }
        stop.store(true, std::memory_order_relaxed);
    });
 
    std::thread reader([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto result = store.get("key");
            if (result.has_value() && result.value().rfind("value_", 0) != 0) {
                corrupted_reads.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });
 
    writer.join();
    reader.join();
 
    EXPECT_EQ(corrupted_reads.load(), 0);
}
 
TEST(StoreConcurrencyTest, ConcurrentSetAndDelSameKey) {
    constexpr int kIterations = 2000;
    Store store;
 
    std::atomic<bool> stop{false};
    std::atomic<int> corrupted_reads{0};
 
    std::thread writer([&] {
        for (int i = 0; i < kIterations; ++i) {
            store.set("key", "value_" + std::to_string(i));
            store.del("key");
        }
        stop.store(true, std::memory_order_relaxed);
    });
 
    std::thread reader([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            auto result = store.get("key");
            if (result.has_value() && result.value().rfind("value_", 0) != 0) {
                corrupted_reads.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });
 
    writer.join();
    reader.join();
 
    EXPECT_EQ(corrupted_reads.load(), 0);
}
 
TEST(StoreConcurrencyTest, ConcurrentMixedWorkload) {
    constexpr int kThreadCount = 6;
    constexpr int kOpsPerThread = 1000;
    constexpr int kKeyCount = 10;
 
    Store store;
    std::latch start{kThreadCount};
    std::atomic<int> corrupted_reads{0};
 
    auto worker = [&](int thread_id) {
        start.count_down();
        start.wait();
 
        unsigned seed = static_cast<unsigned>(thread_id) * 2654435761u + 1;
        auto next = [&seed] {
            seed = seed * 1103515245u + 12345u;
            return seed;
        };
 
        for (int op = 0; op < kOpsPerThread; ++op) {
            std::string key = "key_" + std::to_string(next() % kKeyCount);
            switch (next() % 3) {
                case 0:
                    store.set(key, "value_" + std::to_string(thread_id));
                    break;
                case 1: {
                    auto result = store.get(key);
                    if (result.has_value() &&
                        result.value().rfind("value_", 0) != 0) {
                        corrupted_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                    break;
                }
                case 2:
                    store.del(key);
                    break;
            }
        }
    };
 
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
 
    EXPECT_EQ(corrupted_reads.load(), 0);
}


}  // namespace