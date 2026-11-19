#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include "cse/dump_writer.h"
#include "cse/logger.h"
#include "cse/numbers.h"
#include "cse/session.h"
#include "cse/stop_source.h"

namespace cse {

inline session_factory make_session_factory(locked_repository& repo, ILogger& log, IStopSource& stop) {
    return [&repo, &log, &stop](boost::asio::ip::tcp::socket socket) {
        return std::make_shared<session>(std::move(socket), repo, log, stop);
    };
}

class tcp_server {
public:
    // bind в конструкторе: занятый порт — исключение
    tcp_server(boost::asio::io_context& io, uint16_t port, session_factory make_session, ILogger& log)
        : acceptor_(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::any(), port)),
          make_session_(std::move(make_session)),
          log_(log) {
        do_accept();
    }

    tcp_server(const tcp_server&) = delete;
    tcp_server& operator=(const tcp_server&) = delete;

    uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    // Остановка приёма: pending accept завершается operation_aborted, новый не взводится.
    void stop() {
        boost::system::error_code ec;
        acceptor_.close(ec);
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
            if (ec == boost::asio::error::operation_aborted) {
                return;  // acceptor закрыт: взводить приём снова нельзя
            }
            if (ec) {
                // Транзиентная ошибка не должна навсегда выключать приём.
                log_.error("accept error: " + ec.message());
            } else {
                make_session_(std::move(socket))->start();
            }
            do_accept();
        });
    }

    boost::asio::ip::tcp::acceptor acceptor_;
    session_factory make_session_;
    ILogger& log_;
};

// Facade: main знает только start/stop и порт.
class server_app {
public:
    server_app(uint16_t port, std::chrono::seconds dump_interval,
               std::filesystem::path dump_file, ILogger& log)
        : repo_(),
          stop_(),
          io_(),
          server_(io_, port, make_session_factory(repo_, log, stop_), log),
          dumper_(std::move(dump_file), dump_interval, [this] { return repo_.snapshot(); }, log) {
        // Все операции с сокетами — в io-потоке, поэтому закрытие accept'а постится.
        stop_.subscribe([this] { boost::asio::post(io_, [this] { server_.stop(); }); });
    }

    ~server_app() {
        stop();
    }

    server_app(const server_app&) = delete;
    server_app& operator=(const server_app&) = delete;

    void start() {
        if (io_thread_.joinable()) {
            return;  // второй io-поток не запускаем
        }
        dumper_.start();
        io_thread_ = std::thread([this] { io_.run(); });
    }

    // Идемпотентна и безопасна до start(): просто доводит stop-source и дампер до остановки.
    void stop() {
        stop_.request_stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        dumper_.stop();
    }

    uint16_t port() const {
        return server_.port();
    }

    locked_repository& repo() {
        return repo_;
    }

private:
    locked_repository repo_;
    cse::stop_source stop_;
    boost::asio::io_context io_;
    tcp_server server_;
    dump_writer dumper_;
    std::thread io_thread_;
};

}  // namespace cse
