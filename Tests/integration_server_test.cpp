#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <thread>
#include <vector>

#include "cse/server.h"
#include "support/test_support.h"

namespace cse::test {
namespace {

using namespace std::chrono_literals;

constexpr std::chrono::seconds DUMP_INTERVAL{10};
constexpr std::chrono::seconds FAST_DUMP_INTERVAL{1};

bool wait_until(const std::function<bool()>& predicate, std::chrono::seconds timeout = std::chrono::seconds{5}) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}

bool dump_has_size(const std::filesystem::path& file, std::size_t expected_count) {
    return std::filesystem::exists(file) && std::filesystem::file_size(file) == expected_count * sizeof(uint32_t);
}

}  // namespace

TEST(IntegrationServer, SingleClientSeesGlobalMean) {
    const std::filesystem::path dir = make_temp_dir("server_single");
    const std::filesystem::path dump_file = dir / "numbers.dmp";
    RecordingLogger log;
    server_app app(0, DUMP_INTERVAL, dump_file, log);
    app.start();

    test_client client("127.0.0.1", app.port());
    EXPECT_EQ(client.query(5), 25u);
    EXPECT_EQ(client.query(5), 25u);  // дубликат
    EXPECT_EQ(client.query(7), 37u);
    EXPECT_EQ(app.repo().size(), 2u);

    client.close();
    app.stop();
    EXPECT_EQ(app.repo().size(), 2u);

    // Финальный дамп на stop(): бинарный uint32 по возрастанию.
    ASSERT_TRUE(std::filesystem::exists(dump_file));
    EXPECT_EQ(std::filesystem::file_size(dump_file), 2 * sizeof(uint32_t));
    EXPECT_EQ(read_dump(dump_file), (std::vector<uint32_t>{5, 7}));
}

TEST(IntegrationServer, WritesDumpWhileRunning) {
    const std::filesystem::path dir = make_temp_dir("server_dump_live");
    const std::filesystem::path dump_file = dir / "numbers.dmp";
    RecordingLogger log;
    server_app app(0, FAST_DUMP_INTERVAL, dump_file, log);
    app.start();

    test_client client("127.0.0.1", app.port());
    EXPECT_EQ(client.query(9), 81u);

    EXPECT_TRUE(wait_until([&dump_file] { return dump_has_size(dump_file, 1); }))
        << "дамп не появился, пока сервер работает";
    EXPECT_EQ(read_dump(dump_file), (std::vector<uint32_t>{9}));

    client.close();
    app.stop();
}

TEST(IntegrationServer, TwoClientsShareStore) {
    const std::filesystem::path dir = make_temp_dir("server_two");
    RecordingLogger log;
    server_app app(0, DUMP_INTERVAL, dir / "numbers.dmp", log);
    app.start();

    test_client first("127.0.0.1", app.port());
    test_client second("127.0.0.1", app.port());
    EXPECT_EQ(first.query(5), 25u);
    EXPECT_EQ(second.query(100), (25u + 10000u) / 2u);   // 100 виден первому клиенту
    EXPECT_EQ(first.query(3), (25u + 10000u + 9u) / 3u);  // и наоборот

    first.close();
    second.close();
    app.stop();
}

TEST(IntegrationServer, StopWithConnectedClientIsFast) {
    const std::filesystem::path dir = make_temp_dir("server_stop");
    RecordingLogger log;
    server_app app(0, DUMP_INTERVAL, dir / "numbers.dmp", log);
    app.start();

    test_client client("127.0.0.1", app.port());
    EXPECT_EQ(client.query(5), 25u);

    const auto stop_begin = std::chrono::steady_clock::now();
    app.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - stop_begin, std::chrono::seconds{2});

    const auto read_begin = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.read_mean().has_value());  // клиент получил EOF
    EXPECT_LT(std::chrono::steady_clock::now() - read_begin, std::chrono::seconds{2});

    client.close();
}

}  // namespace cse::test
