#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include "cse/session.h"
#include "support/test_support.h"

namespace cse::test {
namespace {

using namespace std::chrono_literals;

constexpr int CYCLES = 50;

uint64_t query(boost::asio::ip::tcp::socket& socket, uint32_t number) {
    uint8_t request[protocol::REQUEST_SIZE];
    protocol::encode_request(number, request, sizeof(request));
    boost::asio::write(socket, boost::asio::buffer(request, protocol::REQUEST_SIZE));

    uint8_t response[protocol::RESPONSE_SIZE];
    boost::asio::read(socket, boost::asio::buffer(response, protocol::RESPONSE_SIZE));
    uint64_t mean = 0;
    protocol::decode_response(response, protocol::RESPONSE_SIZE, mean);
    return mean;
}

}  // namespace

TEST(Session, AnswersWithMeanOfSquares) {
    boost::asio::io_context io;
    auto [server_socket, client_socket] = make_loopback_pair(io);
    locked_repository repo;
    RecordingLogger log;
    ManualStopSource stop;

    const auto server_session = std::make_shared<session>(std::move(server_socket), repo, log, stop);
    server_session->start();
    std::thread io_thread([&io] { io.run(); });

    EXPECT_EQ(query(client_socket, 7), 49u);
    EXPECT_EQ(query(client_socket, 7), 49u);  // дубликат среднее не меняет

    client_socket.close();
    io_thread.join();  // завершился сам: висящих хендлеров не осталось
}

TEST(Session, PartialRequestClosesSessionQuietly) {
    boost::asio::io_context io;
    auto [server_socket, client_socket] = make_loopback_pair(io);
    locked_repository repo;
    RecordingLogger log;
    ManualStopSource stop;

    const auto server_session = std::make_shared<session>(std::move(server_socket), repo, log, stop);
    server_session->start();
    std::thread io_thread([&io] { io.run(); });

    const uint8_t two_bytes[2]{7, 0};
    boost::asio::write(client_socket, boost::asio::buffer(two_bytes));
    client_socket.close();

    io_thread.join();
    EXPECT_EQ(repo.size(), 0u);
}

TEST(Session, StopClosesClientConnection) {
    boost::asio::io_context io;
    auto [server_socket, client_socket] = make_loopback_pair(io);
    locked_repository repo;
    RecordingLogger log;
    ManualStopSource stop;

    const auto server_session = std::make_shared<session>(std::move(server_socket), repo, log, stop);
    server_session->start();
    std::thread io_thread([&io] { io.run(); });

    EXPECT_EQ(query(client_socket, 3), 9u);
    stop.request_stop();

    uint8_t response[protocol::RESPONSE_SIZE];
    boost::system::error_code ec;
    boost::asio::read(client_socket, boost::asio::buffer(response), ec);
    EXPECT_TRUE(ec);  // клиент увидел EOF вместо зависания

    client_socket.close();
    io_thread.join();
}

TEST(Session, SubscribersDoNotGrowAfterDisconnects) {
    locked_repository repo;
    RecordingLogger log;
    ManualStopSource stop;
    boost::asio::io_context io;

    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        auto [server_socket, client_socket] = make_loopback_pair(io);
        const auto server_session = std::make_shared<session>(std::move(server_socket), repo, log, stop);
        server_session->start();
        io.restart();
        std::thread io_thread([&io] { io.run(); });

        EXPECT_EQ(query(client_socket, 7), 49u);
        client_socket.close();
        io_thread.join();  // сессия закрылась сама
        EXPECT_EQ(stop.subscriber_count(), 0u) << "cycle " << cycle;
    }
}

}  // namespace cse::test
