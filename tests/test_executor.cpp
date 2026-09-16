#include <gtest/gtest.h>

#include "executor.hpp"
#include "stats.hpp"
#include "store.hpp"
#include "thread_pool.hpp"
#include "wal.hpp"

#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

class ExecutorTest : public ::testing::Test {
protected:
    ExecutorTest() : wal_path_("test_executor.wal"), wal_(wal_path_), pool_(2), ctx_{store_, wal_, stats_, pool_} {}

    ~ExecutorTest() override { std::remove(wal_path_.c_str()); }

    std::string run(const std::string& line) {
        return execute(parse_command(line), ctx_);
    }

    std::vector<std::string> lines(const std::string& response) {
        std::vector<std::string> out;
        std::istringstream stream(response);
        std::string line;
        while (std::getline(stream, line)) {
            out.push_back(line);
        }
        return out;
    }

    std::string wal_path_;
    Store store_;
    Wal wal_;
    ThreadPool pool_;
    Stats stats_;
    ServerContext ctx_;
};

TEST_F(ExecutorTest, SetGetDel) {
    EXPECT_EQ(run("SET a hello world"), "OK\n");
    EXPECT_EQ(run("GET a"), "VALUE hello world\n");
    EXPECT_EQ(run("DEL a"), "OK\n");
    EXPECT_EQ(run("GET a"), "NOT_FOUND\n");
    EXPECT_EQ(run("DEL a"), "NOT_FOUND\n");
}

TEST_F(ExecutorTest, Ping) {
    EXPECT_EQ(run("PING"), "PONG\n");
}

TEST_F(ExecutorTest, DbSize) {
    EXPECT_EQ(run("DBSIZE"), "SIZE 0\n");
    run("SET a 1");
    run("SET b 2");
    EXPECT_EQ(run("DBSIZE"), "SIZE 2\n");
}

TEST_F(ExecutorTest, InvalidCommandReturnsError) {
    const std::string response = run("FOO");
    EXPECT_TRUE(response.starts_with("ERROR: "));
    EXPECT_TRUE(response.ends_with("\n"));
}

TEST_F(ExecutorTest, ScanHeaderMatchesLineCount) {
    for (int i = 0; i < 50; ++i) {
        run("SET key" + std::to_string(i) + " value" + std::to_string(i));
    }

    const std::vector<std::string> response = lines(run("SCAN 0 10"));
    ASSERT_FALSE(response.empty());

    std::size_t declared = 0;
    std::size_t next_cursor = 0;
    std::istringstream header(response[0]);
    std::string word;
    header >> word >> declared >> next_cursor;
    EXPECT_EQ(word, "ITEMS");
    EXPECT_EQ(response.size(), declared + 1);

    for (std::size_t i = 1; i < response.size(); ++i) {
        EXPECT_TRUE(response[i].starts_with("KV "));
    }
}

TEST_F(ExecutorTest, ScanFullIterationVisitsEveryKey) {
    std::set<std::string> expected;
    for (int i = 0; i < 200; ++i) {
        const std::string key = "key" + std::to_string(i);
        expected.insert(key);
        run("SET " + key + " v");
    }

    std::set<std::string> seen;
    std::size_t cursor = 0;
    int iterations = 0;

    do {
        const std::vector<std::string> response = lines(run("SCAN " + std::to_string(cursor) + " 7"));
        std::istringstream header(response[0]);
        std::string word;
        std::size_t declared = 0;
        header >> word >> declared >> cursor;

        for (std::size_t i = 1; i < response.size(); ++i) {
            std::istringstream item(response[i]);
            std::string kv;
            std::string key;
            item >> kv >> key;
            seen.insert(key);
        }

        ASSERT_LT(++iterations, 1000) << "курсор не сходится к нулю";
    } while (cursor != 0);

    EXPECT_EQ(seen, expected);
}

TEST_F(ExecutorTest, ScanOnEmptyStoreTerminates) {
    EXPECT_EQ(run("SCAN 0 10"), "ITEMS 0 0\n");
}

TEST_F(ExecutorTest, ScanWithGarbageCursorTerminates) {
    run("SET a 1");
    EXPECT_EQ(run("SCAN 999999 10"), "ITEMS 0 0\n");
}

TEST_F(ExecutorTest, ScanCountIsCapped) {
    for (int i = 0; i < 20; ++i) {
        run("SET key" + std::to_string(i) + " v");
    }
    const std::vector<std::string> response = lines(run("SCAN 0 1000000"));
    std::istringstream header(response[0]);
    std::string word;
    std::size_t declared = 0;
    header >> word >> declared;
    EXPECT_LE(declared, kMaxScanCount + 1);
}

TEST_F(ExecutorTest, InfoHeaderMatchesFieldCount) {
    const std::vector<std::string> response = lines(run("INFO"));
    ASSERT_FALSE(response.empty());

    std::istringstream header(response[0]);
    std::string word;
    std::size_t declared = 0;
    header >> word >> declared;
    EXPECT_EQ(word, "FIELDS");
    EXPECT_EQ(response.size(), declared + 1);

    for (std::size_t i = 1; i < response.size(); ++i) {
        EXPECT_NE(response[i].find(':'), std::string::npos);
    }
}

TEST_F(ExecutorTest, InfoCountsCommands) {
    run("SET a 1");
    run("GET a");
    run("GET a");
    run("FOO");

    const std::string info = run("INFO");
    EXPECT_NE(info.find("cmd_set:1\n"), std::string::npos);
    EXPECT_NE(info.find("cmd_get:2\n"), std::string::npos);
    EXPECT_NE(info.find("cmd_invalid:1\n"), std::string::npos);
    EXPECT_NE(info.find("keys:1\n"), std::string::npos);
}

} // namespace