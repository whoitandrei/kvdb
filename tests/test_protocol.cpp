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


TEST(ProtocolParseTest, TrimsCarriageReturn) {
    Command cmd = parse_command("GET key\r");
    EXPECT_EQ(cmd.type, CommandType::kGet);
    EXPECT_EQ(cmd.key, "key");
}

TEST(ProtocolParseTest, TrimsLeadingAndTrailingSpaces) {
    Command cmd = parse_command("   SET key value   ");
    EXPECT_EQ(cmd.type, CommandType::kSet);
    EXPECT_EQ(cmd.key, "key");
    EXPECT_EQ(cmd.value, "value");
}

TEST(ProtocolParseTest, ParseWhitespaceOnlyLine) {
    EXPECT_EQ(parse_command("   ").type, CommandType::kInvalid);
    EXPECT_EQ(parse_command("\r").type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, CommandNameIsCaseInsensitive) {
    EXPECT_EQ(parse_command("get key").type, CommandType::kGet);
    EXPECT_EQ(parse_command("Set key value").type, CommandType::kSet);
}

TEST(ProtocolParseTest, ParsePing) {
    EXPECT_EQ(parse_command("PING").type, CommandType::kPing);
    EXPECT_EQ(parse_command("PING\r").type, CommandType::kPing);
    EXPECT_EQ(parse_command("PING extra").type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseDbSizeAndInfo) {
    EXPECT_EQ(parse_command("DBSIZE").type, CommandType::kDbSize);
    EXPECT_EQ(parse_command("INFO").type, CommandType::kInfo);
    EXPECT_EQ(parse_command("INFO all").type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, ParseValidScan) {
    Command cmd = parse_command("SCAN 17 100");
    EXPECT_EQ(cmd.type, CommandType::kScan);
    EXPECT_EQ(cmd.cursor, 17u);
    EXPECT_EQ(cmd.count, 100u);
}

TEST(ProtocolParseTest, ParseInvalidScan) {
    EXPECT_EQ(parse_command("SCAN").type, CommandType::kInvalid);
    EXPECT_EQ(parse_command("SCAN 0").type, CommandType::kInvalid);
    EXPECT_EQ(parse_command("SCAN 0 0").type, CommandType::kInvalid);
    EXPECT_EQ(parse_command("SCAN x 10").type, CommandType::kInvalid);
    EXPECT_EQ(parse_command("SCAN -1 10").type, CommandType::kInvalid);
    EXPECT_EQ(parse_command("SCAN 0 10 extra").type, CommandType::kInvalid);
}

TEST(ProtocolParseTest, InvalidCommandCarriesMessage) {
    Command cmd = parse_command("FOO bar");
    EXPECT_EQ(cmd.type, CommandType::kInvalid);
    EXPECT_FALSE(cmd.error_message.empty());
}

}  // namespace