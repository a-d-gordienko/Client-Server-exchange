#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "cse/options.h"

namespace cse::test {
namespace {

std::optional<options> parse_options(const std::vector<std::string>& args) {
    std::string program = "cse_tests";
    std::vector<char*> argv{program.data()};
    for (const std::string& arg : args) {
        argv.push_back(const_cast<char*>(arg.data()));
    }
    return options::parse(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(Options, DefaultsWhenNoArguments) {
    const std::optional<options> opts = parse_options({});
    ASSERT_TRUE(opts.has_value());
    EXPECT_EQ(opts->host, "127.0.0.1");
    EXPECT_EQ(opts->port, 64000);
    EXPECT_EQ(opts->dump_interval, std::chrono::seconds{5});
}

TEST(Options, ParsesPortAndIntervalAndHost) {
    const auto opts = parse_options({"--host", "1.2.3.4", "--port", "7000", "--dump-interval", "10"});
    ASSERT_TRUE(opts.has_value());
    EXPECT_EQ(opts->host, "1.2.3.4");
    EXPECT_EQ(opts->port, 7000);
    EXPECT_EQ(opts->dump_interval, std::chrono::seconds{10});
}

TEST(Options, RejectsNonNumericPort) {
    EXPECT_FALSE(parse_options({"--port", "abc"}).has_value());
}

TEST(Options, RejectsPortOutOfRange) {
    EXPECT_FALSE(parse_options({"--port", "70000"}).has_value());
}

TEST(Options, RejectsZeroPort) {
    EXPECT_FALSE(parse_options({"--port", "0"}).has_value());
}

TEST(Options, RejectsZeroDumpInterval) {
    EXPECT_FALSE(parse_options({"--dump-interval", "0"}).has_value());
}

TEST(Options, RejectsHugeNumber) {
    EXPECT_FALSE(parse_options({"--port", "99999999999999999999"}).has_value());
    EXPECT_FALSE(parse_options({"--dump-interval", "99999999999999999999"}).has_value());
}

TEST(Options, RejectsNegativeNumber) {
    EXPECT_FALSE(parse_options({"--port", "-1"}).has_value());
    EXPECT_FALSE(parse_options({"--dump-interval", "-1"}).has_value());
}

TEST(Options, RejectsTrailingGarbage) {
    EXPECT_FALSE(parse_options({"--port", "12abc"}).has_value());
    EXPECT_FALSE(parse_options({"--dump-interval", "12abc"}).has_value());
}

TEST(Options, RejectsEmptyHost) {
    EXPECT_FALSE(parse_options({"--host", ""}).has_value());
}

TEST(Options, RejectsUnknownFlag) {
    EXPECT_FALSE(parse_options({"--unknown", "1"}).has_value());
}

TEST(Options, RejectsFlagWithoutValue) {
    EXPECT_FALSE(parse_options({"--port"}).has_value());
}

}  // namespace cse::test
