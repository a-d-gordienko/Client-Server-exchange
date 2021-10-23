#pragma once

#include <iostream>
#include <string>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <cstdint>
#include <queue>
#include <chrono>
#include "Logger.h"
#include "connect.h"
#include "SrvAlg.h"
#include "dumper.h"

namespace srv {
    using namespace con;
    using boost::asio::ip::tcp;
    using namespace std::chrono;

    class client_io final {
        struct data_block {
            data_block() {}
            data_block(tcp_connection::pointer pt_) : pt(pt_){}
            data_block(const data_block& db) : pt(db.pt){}
            tcp_connection::pointer pt;
        };
    public:
        client_io() {
            thr_writer = std::thread(std::bind(&client_io::thr_func, this));
        }

        ~client_io() {
            is_work.store(false);
            thr_writer.join();
        }

        bool start_io(tcp_connection::pointer pt) {
            pq.push(data_block(pt));
            return true;
        }

    private:
        void thr_func() {
            log_write->info("client_io::thr_func start thread clients input/output");
            is_work.store(true);
            uint32_t connection_number{ 0 };
            uint64_t start_time = get_tick_count();
            for (;;) {
                if (!is_work.load()) {
                    break;
                }
                data_block db;
                if (pq.pop(db)) {
                    connections[connection_number++] = db.pt;
                }
                read_from_connections();
                get_data_after_read();
                uint64_t tick{};
                if ((tick = get_tick_count()) >= start_time + (srv::DUMP_TIMEOUT * 1000)) {
                    dump_connections();
                    start_time = tick;
                }
                write_to_connections();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            log_write->info("client_io::thr_func end thread clients input/output");
        }

        void read_from_connections() {
            for (auto it = connections.begin(); it != connections.end();) {
                if (it->second->is_open() && it->second->is_read() == RW_STATUS::UNKNOWN || it->second->is_read() == RW_STATUS::COMPLETE) {
                    it->second->read();
                    ++it;
                }
                else if (it->second->is_read() == RW_STATUS::CONNECTION_CLOSE) {
                    log_write->info("client_io::read_from_connections: connection {} in status:{} delete connection", it->first, rw_status_strs[it->second->is_read()].c_str());
                    erase_connection_by_iterator(it);
                }
            }
        }

        void get_data_after_read() {
            for (auto it = connections.begin(); it != connections.end();) {
               if (it->second->is_read() == RW_STATUS::COMPLETE) {
                   size_t sz_data = it->second->data_size();
                   uint32_t number = *(uint32_t*)it->second->get_data();
                   log_write->info("client_io::get_data_after_read: connection {} in status:{} read, size data:{} number:{} transfer to storage", it->first, rw_status_strs[it->second->is_read()].c_str(), sz_data, number);
                   storage.to_storage(it->first, number);
                   ++it;
               }
               else if (it->second->is_read() == RW_STATUS::CONNECTION_CLOSE) {
                   log_write->info("client_io::get_data_after_read: connection {} in status:{} delete connection", it->first, rw_status_strs[it->second->is_read()].c_str());
                   erase_connection_by_iterator(it);
               }
           }
        }

        void write_to_connections() {
            for (auto it = connections.begin(); it != connections.end();) {
                if (it->second->is_open() && it->second->is_write() == RW_STATUS::UNKNOWN || it->second->is_write() == RW_STATUS::COMPLETE) {
                    uint64_t arithmetic_mean{};
                    if (storage.get_arithmetic_mean(it->first, arithmetic_mean)) {              //считаем среднее арифметическое квадратов
                        log_write->info("client_io::write_to_connections connect:{} status {} send number:{}", it->first, rw_status_strs[it->second->is_write()], arithmetic_mean);
                        it->second->write((uint8_t*)&arithmetic_mean, sizeof(arithmetic_mean)); //передаем клиенту высчитанное число
                    }
                    ++it;
                }
                else if (it->second->is_write() == RW_STATUS::CONNECTION_CLOSE) {
                    log_write->info("client_io::write_to_connections: connection {} in status:{} delete connection", it->first, rw_status_strs[it->second->is_write()].c_str());
                    erase_connection_by_iterator(it);
                }
            }
        }

        void erase_connection_by_iterator(std::unordered_map<uint32_t, tcp_connection::pointer>::iterator &it) {
            auto del_it = it;
            ++it;
            connections.erase(del_it);
        }
        void dump_connections() {
            for (auto connection : connections) {
                if (connection.second->is_read() == RW_STATUS::COMPLETE) {
                    log_write->info("client_io::dump_connections connect:{} in status{} transfer to dumper thread", connection.first, rw_status_strs[RW_STATUS::COMPLETE]);
                    std::unordered_set<uint64_t> nums;
                    if (storage.get_storage(connection.first, nums)) {
                        dwriter.to_dump(connection.first, nums);
                    }
                }
            }
        }

        uint64_t get_tick_count() {
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

    private:
        std::unordered_map<uint32_t, tcp_connection::pointer> connections;
        salg::parallel_queue<data_block> pq;
        std::thread thr_writer{};
        std::atomic_bool is_work{ false };
        salg::storage_numbers storage;
        dump_writer dwriter;
    };

    class tcp_server 
    {
    public:
        tcp_server(boost::asio::io_context& io_context, uint16_t port)
            : io_context_(io_context),
            acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
            start_accept();
        }

    private:
        void start_accept()
        {
            log_write->info("start accept client");
            tcp_connection::pointer new_connection =
                tcp_connection::create(io_context_, 4);

            acceptor_.async_accept(new_connection->socket(),
                boost::bind(&tcp_server::handle_accept, this, new_connection,
                    boost::asio::placeholders::error));
            
        }
        std::shared_ptr<std::string> message_;
        void handle_accept(tcp_connection::pointer new_connection,
            const boost::system::error_code& error)
        {
            log_write->info("accept new connection error:{} message:{}",error.value(), error.message().c_str());
            if (!error)
            {
                cio.start_io(new_connection);
            }

            start_accept();
        }

    protected:
        boost::asio::io_context& io_context_;
        tcp::acceptor acceptor_;
        client_io cio;
    };

    constexpr uint16_t DEFAULT_PORT = 64000;
    
    class srv_mgr final {
    public:

        srv_mgr(const srv_mgr&) = delete;
        srv_mgr(srv_mgr&&) = delete;
        srv_mgr& operator=(const srv_mgr&) = delete;
        srv_mgr& operator= (srv_mgr&&) = delete;

        static srv_mgr& instance() {
            static srv_mgr smgr;
            return smgr;
        }
        void start(uint16_t port_ = DEFAULT_PORT) {
            port = port_;
            thr_mgr = std::thread(std::bind(&srv_mgr::thread_func, this));
        }
        void stop() {
            io_context.stop();
            thr_mgr.join();
        }
    private:
        srv_mgr() = default;
        void thread_func() {
            log_write->info("start server manager thread function");
            try{
                srv::tcp_server server(io_context, port);
                io_context.run();
            }
            catch (std::exception& e)
            {
               log_write->error(e.what());
            }
            catch (...) {
                log_write->error("unknown exception");
            }
            log_write->info("end server manager thread function");
        }
    private:
        boost::asio::io_context io_context;
        uint16_t port{ DEFAULT_PORT };
        std::thread thr_mgr{};
    };
}
#define SERVER srv::srv_mgr::instance()

