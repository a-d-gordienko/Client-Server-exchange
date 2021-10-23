#pragma once
#include "Logger.h"
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/asio.hpp>
namespace con {
    
    using boost::asio::ip::tcp;

    enum RW_STATUS : uint8_t {
        UNKNOWN = 0xff,
        COMPLETE = 0,
        IN_PROGRESS = 1,
        CONNECTION_CLOSE = 2
    };

    std::unordered_map<RW_STATUS, std::string> rw_status_strs{
        {UNKNOWN,"UNKNOWN"},
        { COMPLETE,"COMPLETE"},
        { IN_PROGRESS , "IN_PROGRESS"},
        { CONNECTION_CLOSE , "CONNECTION_CLOSE"}
    };

    class tcp_connection : public boost::enable_shared_from_this<tcp_connection>{
    public:

        using pointer = boost::shared_ptr<tcp_connection> ;

        static pointer create(boost::asio::io_context& io_context, size_t sz_read_buff)
        {
            return pointer(new tcp_connection(io_context, sz_read_buff));
        }

        tcp::socket& socket()
        {
            return socket_;
        }

        void read() {
            is_current_read_end.store(RW_STATUS::IN_PROGRESS);
            memset(rd_buff.get(), 0, rd_buff_sz);
            read_data_sz = 0;
            boost::asio::async_read(socket_, boost::asio::buffer(rd_buff.get(), rd_buff_sz),
                boost::bind(&tcp_connection::handle_read, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));

        }

        void write(uint8_t* data, size_t sz){
            is_current_write_end.store(RW_STATUS::IN_PROGRESS);
            boost::asio::async_write(socket_, boost::asio::buffer(data, sz),
                boost::bind(&tcp_connection::handle_write, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
        }

        RW_STATUS is_read() {
            return is_current_read_end.load();
        }

        RW_STATUS is_write() {
            return is_current_write_end.load();
        }

        bool is_open() {
            return socket_.is_open();
        }

        size_t data_size() {
            return read_data_sz;
        }
        uint8_t* get_data() {
            return rd_buff.get();
        }
    private:
        tcp_connection(boost::asio::io_context& io_context, size_t sz_read_buff)
            : socket_(io_context) {
            rd_buff = std::make_unique<uint8_t[]>(sz_read_buff);
            rd_buff_sz = sz_read_buff;
        }

        void handle_read(const boost::system::error_code& error, size_t bytes_transferred) {
            log_write->info("tcp_connection::handle_read: bytes transfered={} value={} string={}", bytes_transferred, error.value(), error.message().c_str());
            if (error.value() == WSAECONNRESET || error.value() == WSAECONNABORTED) {
                is_current_read_end.store(RW_STATUS::CONNECTION_CLOSE);
            }
            else {
                is_current_read_end.store(RW_STATUS::COMPLETE);
            }
            read_data_sz = bytes_transferred;
        }

        void handle_write(const boost::system::error_code& error, size_t bytes_transferred) {
            log_write->info("tcp_connection::handle_write: bytes transfered={} value={} string={}", bytes_transferred, error.value(), error.message().c_str());
            if (error.value() == WSAECONNRESET || error.value() == WSAECONNABORTED) {
                is_current_write_end.store(RW_STATUS::CONNECTION_CLOSE);
            }
            else {
                is_current_write_end.store(RW_STATUS::COMPLETE);
            }
        }
    private:
        tcp::socket socket_;
        std::unique_ptr<uint8_t[]> rd_buff{};
        std::atomic<RW_STATUS> is_current_read_end{ RW_STATUS::UNKNOWN };
        std::atomic<RW_STATUS> is_current_write_end{ RW_STATUS::UNKNOWN };
        size_t rd_buff_sz{};
        size_t read_data_sz{};
    };
}
