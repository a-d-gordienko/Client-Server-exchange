#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio.hpp>

#include "cse/logger.h"
#include "cse/mean_of_squares.h"
#include "cse/numbers.h"
#include "cse/protocol.h"
#include "cse/stop_source.h"

namespace cse {

// Цикл одного соединения: read 4 -> add -> mean -> write 8 -> read ...
class session : public std::enable_shared_from_this<session> {
public:
    using pointer = std::shared_ptr<session>;

    session(boost::asio::ip::tcp::socket socket, locked_repository& repo, ILogger& log, IStopSource& stop)
        : socket_(std::move(socket)), repo_(repo), log_(log), stop_(stop) {}

    void start() {
        // Остановка сервера закрывает сокет внутри io-потока: клиент получает EOF.
        subscription_ = stop_.subscribe([weak = std::weak_ptr<session>(shared_from_this())] {
            if (const pointer self = weak.lock()) {
                boost::asio::post(self->socket_.get_executor(), [self] { self->close(); });
            }
        });
        do_read();
    }

private:
    using error_code = boost::system::error_code;

    void close() {
        stop_.unsubscribe(subscription_);
        error_code ec;
        socket_.close(ec);
    }

    void do_read() {
        const pointer self = shared_from_this();
        boost::asio::async_read(socket_, boost::asio::buffer(req_buf_, protocol::REQUEST_SIZE),
                                [self](const error_code& ec, std::size_t) { self->on_read(ec); });
    }

    void on_read(const error_code& ec) {
        if (ec) {
            finish(ec);
            return;
        }
        // Исключение не должно вылетать из io_context::run(): сервер живёт дальше.
        try {
            uint32_t number = 0;
            if (!protocol::decode_request(req_buf_, protocol::REQUEST_SIZE, number)) {
                close();
                return;
            }
            repo_.add(number);
            log_.info("server read number:" + std::to_string(number));
            protocol::encode_response(repo_.mean_of_squares().value_or(0), resp_buf_, protocol::RESPONSE_SIZE);
        } catch (const std::exception& error) {
            log_.error(std::string("session error: ") + error.what());
            close();
            return;
        }
        do_write();
    }

    void do_write() {
        const pointer self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(resp_buf_, protocol::RESPONSE_SIZE),
                                 [self](const error_code& ec, std::size_t) { self->on_write(ec); });
    }

    void on_write(const error_code& ec) {
        if (ec) {
            finish(ec);
            return;
        }
        do_read();
    }

    // Обрыв или остановка сервера — закрываем сессию тихо, без повторов.
    void finish(const error_code& ec) {
        close();
        log_.debug("session closed: " + ec.message());
    }

    boost::asio::ip::tcp::socket socket_;
    locked_repository& repo_;
    ILogger& log_;
    IStopSource& stop_;
    std::uint64_t subscription_{0};
    uint8_t req_buf_[protocol::REQUEST_SIZE]{};
    uint8_t resp_buf_[protocol::RESPONSE_SIZE]{};
};

using session_factory = std::function<session::pointer(boost::asio::ip::tcp::socket)>;

}  // namespace cse
