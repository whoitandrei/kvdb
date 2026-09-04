#include <gtest/gtest.h>

#include "store.hpp"
#include "wal.hpp"

#include <filesystem>
#include <fstream>
#include <latch>
#include <thread>
#include <unistd.h>

namespace {

class WalTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        path_ = (std::filesystem::temp_directory_path() /
                  ("wal_test_" + std::to_string(::getpid()) + "_" + test_name + ".wal"))
                     .string();
        std::filesystem::remove(path_);
    }

    void TearDown() override {
        std::filesystem::remove(path_);
    }

    std::string path_;
};

TEST_F(WalTest, AppendSetThenReplayAppliesToStore) {
    {
        Wal wal(path_);
        wal.append_set("key", "value");
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    auto value = store.get("key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value");
}

TEST_F(WalTest, AppendDelAfterSetThenReplayRemovesKey) {
    {
        Wal wal(path_);
        wal.append_set("key", "value");
        wal.append_del("key");
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    EXPECT_EQ(store.get("key"), std::nullopt);
}

TEST_F(WalTest, MultipleRecordsReplayInOrder) {
    {
        Wal wal(path_);
        wal.append_set("a", "1");
        wal.append_set("b", "2");
        wal.append_set("a", "overwritten");
        wal.append_del("b");
        wal.append_set("c", "3");
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    EXPECT_EQ(store.get("a").value(), "overwritten");
    EXPECT_EQ(store.get("b"), std::nullopt);
    EXPECT_EQ(store.get("c").value(), "3");
}

TEST_F(WalTest, ReplayOnEmptyFileDoesNothing) {
    Store store;
    Wal wal(path_);
    wal.replay(store);

    EXPECT_EQ(store.get("anything"), std::nullopt);
}

TEST_F(WalTest, ReplaySurvivesProcessRestartSimulation) {
    {
        Wal wal(path_);
        wal.append_set("x", "1");
        wal.append_set("y", "2");
    }

    {
        Store store;
        Wal wal(path_);
        wal.replay(store);

        EXPECT_EQ(store.get("x").value(), "1");
        EXPECT_EQ(store.get("y").value(), "2");
    }
}

TEST_F(WalTest, ValueWithEmbeddedNullByteSurvivesRoundtrip) {
    std::string value_with_null = std::string("ab") + '\0' + "cd";
    ASSERT_EQ(value_with_null.size(), 5u);  // 'a','b','\0','c','d'

    {
        Wal wal(path_);
        wal.append_set("key", value_with_null);
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    auto result = store.get("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), value_with_null);
    EXPECT_EQ(result.value().size(), 5u);
}

TEST_F(WalTest, EmptyValueRoundtrips) {
    {
        Wal wal(path_);
        wal.append_set("key", "");
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    auto result = store.get("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

TEST_F(WalTest, TruncatedTrailingRecordIsIgnoredOnReplay) {
    {
        Wal wal(path_);
        wal.append_set("complete", "value");
    }

    {
        std::ofstream raw(path_, std::ios::binary | std::ios::app);
        uint32_t fake_len = 100;  // обещаем 100 байт
        raw.write(reinterpret_cast<const char*>(&fake_len), sizeof(fake_len));
        raw.write("only a few bytes", 16);
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    EXPECT_EQ(store.get("complete").value(), "value");
    EXPECT_EQ(store.get("incomplete"), std::nullopt);
}

TEST_F(WalTest, ConcurrentAppendsFromMultipleThreadsAllSurviveReplay) {
    constexpr int kThreadCount = 4;
    constexpr int kOpsPerThread = 200;

    {
        Wal wal(path_);
        std::latch start{kThreadCount};
        std::vector<std::thread> threads;

        for (int t = 0; t < kThreadCount; ++t) {
            threads.emplace_back([&, t] {
                start.count_down();
                start.wait();
                for (int i = 0; i < kOpsPerThread; ++i) {
                    std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
                    wal.append_set(key, "value");
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }
    }

    Store store;
    Wal wal(path_);
    wal.replay(store);

    for (int t = 0; t < kThreadCount; ++t) {
        for (int i = 0; i < kOpsPerThread; ++i) {
            std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i);
            auto value = store.get(key);
            ASSERT_TRUE(value.has_value()) << "missing key: " << key;
            EXPECT_EQ(value.value(), "value");
        }
    }
}

}  // namespace