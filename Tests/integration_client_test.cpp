#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "cse/client.h"
#include "cse/options.h"
#include "cse/server.h"
#include "support/test_support.h"

namespace cse::test {
namespace {

using namespace std::chrono_literals;

constexpr int READ_ATTEMPTS = 3;
constexpr std::chrono::seconds WAIT_LIMIT{5};

bool wait_until(const std::function<bool()>& predicate, std::chrono::seconds timeout = WAIT_LIMIT) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}

uint16_t reserve_closed_port() {
    boost::asio::io_context io;
    const boost::asio::ip::tcp::acceptor acceptor(
        io, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    return acceptor.local_endpoint().port();
}

// Ранний выход из теста (ASSERT) не должен оставлять joinable поток: иначе std::terminate.
class runner_guard {
public:
    runner_guard(client_app& app, std::thread& runner) : app_(app), runner_(runner) {}
    ~runner_guard() {
        app_.request_stop();
        if (runner_.joinable()) {
            runner_.join();
        }
    }

    runner_guard(const runner_guard&) = delete;
    runner_guard& operator=(const runner_guard&) = delete;

private:
    client_app& app_;
    std::thread& runner_;
};

}  // namespace

TEST(IntegrationClient, ThreeFailedAttemptsExitWithError) {
    options opts;
    opts.port = reserve_closed_port();
    RecordingLogger log;
    SequentialRandomSource random;
    client_app app(opts, random, log);

    // На закрытый loopback-порт connect отваливается по SYN-таймауту ОС (~2 с),
    // поэтому граница проверяет отсутствие вечного цикла, а не латентность.
    const auto begin = std::chrono::steady_clock::now();
    EXPECT_EQ(app.run(), 1);
    EXPECT_LT(std::chrono::steady_clock::now() - begin, std::chrono::seconds{10});
    EXPECT_EQ(log.count_errors("client connect error"), 3u);
}

TEST(IntegrationClient, ExchangesWithServerAndStops) {
    const std::filesystem::path dir = make_temp_dir("client_live");
    RecordingLogger server_log;
    RecordingLogger client_log;
    server_app server(0, std::chrono::seconds{10}, dir / "numbers.dmp", server_log);
    server.start();

    options opts;
    opts.port = server.port();
    SequentialRandomSource random;
    client_app app(opts, random, client_log);

    int exit_code = -1;
    std::thread runner([&app, &exit_code] { exit_code = app.run(); });
    const runner_guard guard(app, runner);

    ASSERT_TRUE(wait_until([&client_log] { return client_log.count_infos("client read number:") >= READ_ATTEMPTS; }));

    // SequentialRandomSource: 0, 1, 2 → средние 0, 0, 1
    const std::vector<std::string> infos = client_log.infos();
    ASSERT_GE(infos.size(), 7u);
    EXPECT_EQ(infos[0], "client start");
    EXPECT_EQ(infos[1], "client send number:0");
    EXPECT_EQ(infos[2], "client read number:0");
    EXPECT_EQ(infos[3], "client send number:1");
    EXPECT_EQ(infos[4], "client read number:0");
    EXPECT_EQ(infos[5], "client send number:2");
    EXPECT_EQ(infos[6], "client read number:1");

    const auto stop_begin = std::chrono::steady_clock::now();
    app.request_stop();
    runner.join();
    EXPECT_LT(std::chrono::steady_clock::now() - stop_begin, std::chrono::seconds{2});
    EXPECT_EQ(exit_code, 0);

    server.stop();
}

}  // namespace cse::test
