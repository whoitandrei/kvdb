#include <gtest/gtest.h>

#include "protocol.hpp"

namespace {

TEST(ProtocolParseTest, ParseValidSet) {
    Command cmd = parse_command("SET key value");
    EXPECT_EQ(cmd.type, CommandType::kSet);
    EXPECT_EQ(cmd.key, "key");
    EXPECT_EQ(cmd.value, "value");
}

TEST(ProtocolParseTest, ParseValidGet) {
    Command cmd = parse_command("GET key");
    EXPECT_EQ(cmd.type, CommandType::kGet);
    EXPECT_EQ(cmd.key, "key");
}

TEST(ProtocolParseTest, ParseValidDel) {
    Command cmd = parse_command("DEL key");
    EXPECT_EQ(cmd.type, CommandType::kDel);
    EXPECT_EQ(cmd.key, "key");
}

TEST(ProtocolParseTest, ParseSetWithMultiWordValue) {
    Command cmd = parse_command("SET key hello world foo");
    EXPECT_EQ(cmd.type, CommandType::kSet);
    EXPECT_EQ(cmd.key, "key");
    EXPECT_EQ(cmd.value, "hello world foo");
}

TEST(ProtocolParseTest, ParseUnknownCommand) {
    Command cmd = parse_command("FOO key value");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseEmptyString) {
    Command cmd = parse_command("");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseSetMissingValue) {
    Command cmd = parse_command("SET key");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseSetEmptyKey) {
    Command cmd = parse_command("SET  value");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseSetEmptyValue) {
    Command cmd = parse_command("SET key ");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseGetMissingKey) {
    // Нет пробела вообще — отваливается ещё до сравнения с "GET".
    Command cmd = parse_command("GET");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseGetEmptyKey) {
    Command cmd = parse_command("GET ");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseGetExtraArgument) {
    Command cmd = parse_command("GET key extra");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseDelMissingKey) {
    Command cmd = parse_command("DEL");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseDelEmptyKey) {
    Command cmd = parse_command("DEL ");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseDelExtraArgument) {
    Command cmd = parse_command("DEL key extra");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
}

}  // namespace