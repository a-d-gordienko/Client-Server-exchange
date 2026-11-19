#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <process.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/asio.hpp>

#include "cse/logger.h"
#include "cse/protocol.h"
#include "cse/random_source.h"

namespace cse::test {

constexpr std::chrono::milliseconds CLIENT_TIMEOUT{3000};

// Каталог уникален на процесс: параллельные прогоны не перезаписывают дампы друг друга.
inline std::filesystem::path make_temp_dir(const std::string& name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "cse_test" / (name + "_" + std::to_string(_getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

// Чтение бинарного дампа: uint32 подряд.
inline std::vector<uint32_t> read_dump(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::vector<uint32_t> numbers;
    uint32_t number = 0;
    while (in.read(reinterpret_cast<char*>(&number), sizeof(number))) {
        numbers.push_back(number);
    }
    return numbers;
}

class RecordingLogger final : public ILogger {
public:
    void info(const std::string& msg) override {
        const std::lock_guard lock(mtx_);
        infos_.push_back(msg);
    }
    void error(const std::string& msg) override {
        const std::lock_guard lock(mtx_);
        errors_.push_back(msg);
    }
    void debug(const std::string& msg) override {
        const std::lock_guard lock(mtx_);
        debugs_.push_back(msg);
    }

    std::vector<std::string> infos() const {
        const std::lock_guard lock(mtx_);
        return infos_;
    }

    std::vector<std::string> errors() const {
        const std::lock_guard lock(mtx_);
        return errors_;
    }

    std::size_t count_infos(std::string_view needle) const {
        const std::lock_guard lock(mtx_);
        return count_with(infos_, needle);
    }

    std::size_t count_errors(std::string_view needle) const {
        const std::lock_guard lock(mtx_);
        return count_with(errors_, needle);
    }

private:
    static std::size_t count_with(const std::vector<std::string>& messages, std::string_view needle) {
        return static_cast<std::size_t>(
            std::count_if(messages.begin(), messages.end(),
                          [needle](const std::string& msg) { return msg.find(needle) != std::string::npos; }));
    }

    mutable std::mutex mtx_;
    std::vector<std::string> infos_;
    std::vector<std::string> errors_;
    std::vector<std::string> debugs_;
};

// Strategy для тестов: детерминированная последовательность 0, 1, 2, ...
class SequentialRandomSource final : public IRandomSource {
public:
    explicit SequentialRandomSource(uint32_t start = 0) : next_(start) {}

    uint32_t next() override {
        const uint32_t value = next_;
        next_ = (next_ + 1) % (MAX_NUMBER + 1);
        return value;
    }

private:
    uint32_t next_;
};

// Пара соединённых сокетов на 127.0.0.1:0 для юнит-тестов asio-слоя.
inline std::pair<boost::asio::ip::tcp::socket, boost::asio::ip::tcp::socket> make_loopback_pair(
    boost::asio::io_context& io) {
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor(io, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    tcp::socket client(io);
    client.connect(acceptor.local_endpoint());
    tcp::socket server = acceptor.accept();
    return {std::move(server), std::move(client)};
}

// Синхронный клиент для интеграционных тестов: протокол тот же, что у боевого.
// Дедлайн общий на connect и на чтение: таймер рвёт сокет, чтобы операция не висела.
class test_client {
public:
    test_client(const std::string& host, uint16_t port) : socket_(io_), deadline_(io_) {
        boost::system::error_code address_error;
        const boost::asio::ip::address address = boost::asio::ip::make_address(host, address_error);
        if (address_error) {
            throw boost::system::system_error(address_error, "test_client: bad host");
        }
        const boost::system::error_code connect_error = connect(boost::asio::ip::tcp::endpoint(address, port));
        if (connect_error) {
            throw boost::system::system_error(connect_error, "test_client: connect");
        }
    }

    void send(uint32_t number) {
        uint8_t buffer[protocol::REQUEST_SIZE];
        protocol::encode_request(number, buffer, sizeof(buffer));
        boost::asio::write(socket_, boost::asio::buffer(buffer, protocol::REQUEST_SIZE));
    }

    std::optional<uint64_t> read_mean() {
        uint8_t buffer[protocol::RESPONSE_SIZE];
        boost::system::error_code read_error;
        std::size_t received = 0;
        arm_deadline();
        boost::asio::async_read(socket_, boost::asio::buffer(buffer, protocol::RESPONSE_SIZE),
                                [this, &read_error, &received](const boost::system::error_code& ec, std::size_t size) {
                                    read_error = ec;
                                    received = size;
                                    deadline_.cancel();
                                });
        run_io();

        uint64_t mean = 0;
        if (read_error || received != protocol::RESPONSE_SIZE ||
            !protocol::decode_response(buffer, protocol::RESPONSE_SIZE, mean)) {
            return std::nullopt;
        }
        return mean;
    }

    std::optional<uint64_t> query(uint32_t number) {
        send(number);
        return read_mean();
    }

    void close() {
        boost::system::error_code ec;
        socket_.close(ec);
    }

private:
    boost::system::error_code connect(const boost::asio::ip::tcp::endpoint& endpoint) {
        boost::system::error_code connect_error;
        arm_deadline();
        socket_.async_connect(endpoint, [this, &connect_error](const boost::system::error_code& ec) {
            connect_error = ec;
            deadline_.cancel();
        });
        run_io();
        return connect_error;
    }

    void arm_deadline() {
        deadline_.expires_after(CLIENT_TIMEOUT);
        deadline_.async_wait([this](const boost::system::error_code& ec) {
            if (ec) {
                return;  // операция успела завершиться, таймер отменён
            }
            boost::system::error_code ignored;
            socket_.close(ignored);
        });
    }

    void run_io() {
        io_.restart();
        io_.run();
    }

    boost::asio::io_context io_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer deadline_;
};

}  // namespace cse::test
