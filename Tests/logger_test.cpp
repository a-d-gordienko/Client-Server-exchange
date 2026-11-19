#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "cse/logger.h"
#include "support/test_support.h"

namespace cse::test {

TEST(Logger, WritesFileAndFiltersDebug) {
    const std::filesystem::path file = make_temp_dir("logger") / "test.log";
    {
        const std::shared_ptr<ILogger> logger = SpdlogLogger::create(file.string());
        logger->info("hello");
        logger->debug("dbg");
        logger->error("oops");
    }  // разрушение логгера закрывает файл

    std::ifstream in(file);
    ASSERT_TRUE(in.is_open());
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("hello"), std::string::npos);
    EXPECT_NE(content.find("oops"), std::string::npos);
    EXPECT_EQ(content.find("dbg"), std::string::npos);
}

TEST(Logger, DebugLevelWritesDebugMessages) {
    const std::filesystem::path file = make_temp_dir("logger_debug") / "test.log";
    {
        const std::shared_ptr<ILogger> logger = SpdlogLogger::create(file.string(), spdlog::level::debug);
        logger->debug("dbg");
    }

    std::ifstream in(file);
    ASSERT_TRUE(in.is_open());
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("dbg"), std::string::npos);
}

}  // namespace cse::test
