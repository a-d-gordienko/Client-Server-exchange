#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <boost/asio.hpp>

#include "cse/logger.h"
#include "cse/options.h"
#include "cse/protocol.h"
#include "cse/random_source.h"
#include "cse/stop_source.h"

namespace cse {

// asio-работник: connect (до 3 попыток) -> цикл write 4 / read 8, темп задаёт roundtrip.
class client {
public:
    client(boost::asio::io_context& io, std::string host, uint16_t port,
           IRandomSource& rnd, ILogger& log, IStopSource& stop)
        : io_(io), host_(std::move(host)), port_(port), rnd_(rnd), log_(log), stop_(stop), socket_(io) {}

    void start() {
        do_connect();
    }

    int exit_code() const {
        return exit_code_;
    }

private:
    using error_code = boost::system::error_code;

    static constexpr int MAX_ATTEMPTS = 3;

    void do_connect() {
        socket_ = boost::asio::ip::tcp::socket(io_);
        error_code address_error;
        const boost::asio::ip::address address = boost::asio::ip::make_address(host_, address_error);
        if (address_error) {
            fail(address_error);
            return;
        }
        socket_.async_connect(boost::asio::ip::tcp::endpoint(address, port_),
                              [this](const error_code& ec) { on_connect(ec); });
    }

    void on_connect(const error_code& ec) {
        if (stop_.stop_requested()) {
            finish(0);
            return;
        }
        if (ec) {
            ++attempts_;
            log_.error("client connect error code:" + std::to_string(ec.value()) + " error message:" + ec.message());
            if (attempts_ >= MAX_ATTEMPTS) {
                finish(1);
                return;
            }
            do_connect();
            return;
        }
        attempts_ = 0;  // лимит — на цикл переподключения, а не на всю жизнь процесса
        do_write();
    }

    void do_write() {
        const uint32_t number = rnd_.next();
        log_.info("client send number:" + std::to_string(number));
        protocol::encode_request(number, req_buf_, protocol::REQUEST_SIZE);
        boost::asio::async_write(socket_, boost::asio::buffer(req_buf_, protocol::REQUEST_SIZE),
                                 [this](const error_code& ec, std::size_t) { on_write(ec); });
    }

    void on_write(const error_code& ec) {
        if (ec) {
            fail(ec);
            return;
        }
        do_read();
    }

    void do_read() {
        boost::asio::async_read(socket_, boost::asio::buffer(resp_buf_, protocol::RESPONSE_SIZE),
                                [this](const error_code& ec, std::size_t) { on_read(ec); });
    }

    void on_read(const error_code& ec) {
        if (ec) {
            fail(ec);
            return;
        }
        uint64_t mean = 0;
        if (!protocol::decode_response(resp_buf_, protocol::RESPONSE_SIZE, mean)) {
            finish(1);
            return;
        }
        log_.info("client read number:" + std::to_string(mean));
        do_write();
    }

    // Разрыв связи — переподключение в рамках общего лимита попыток.
    void fail(const error_code& ec) {
        error_code ignored;
        socket_.close(ignored);
        if (stop_.stop_requested()) {
            finish(0);
            return;
        }
        ++attempts_;
        log_.error("client error code:" + std::to_string(ec.value()) + " error message:" + ec.message());
        if (attempts_ >= MAX_ATTEMPTS) {
            finish(1);
            return;
        }
        do_connect();
    }

    void finish(int code) {
        exit_code_ = code;
        io_.stop();
    }

    boost::asio::io_context& io_;
    std::string host_;
    uint16_t port_;
    IRandomSource& rnd_;
    ILogger& log_;
    IStopSource& stop_;
    boost::asio::ip::tcp::socket socket_;
    int attempts_{0};
    int exit_code_{0};
    uint8_t req_buf_[protocol::REQUEST_SIZE]{};
    uint8_t resp_buf_[protocol::RESPONSE_SIZE]{};
};

// Facade: main знает только run/request_stop.
class client_app {
public:
    client_app(const options& opts, IRandomSource& rnd, ILogger& log)
        : log_(log), stop_(), io_(), client_(io_, opts.host, opts.port, rnd, log, stop_) {
        stop_.subscribe([this] { io_.stop(); });
    }

    client_app(const client_app&) = delete;
    client_app& operator=(const client_app&) = delete;

    int run() {
        log_.info("client start");
        client_.start();
        io_.run();
        log_.info("client end");
        return client_.exit_code();
    }

    void request_stop() {
        stop_.request_stop();
    }

private:
    ILogger& log_;
    stop_source stop_;
    boost::asio::io_context io_;
    client client_;
};

}  // namespace cse
