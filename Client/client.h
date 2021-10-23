#pragma once
#include <string>
#include <thread>
#include <functional>
#include <mutex>
#include <boost/asio.hpp>
#include "logger.h"
#include "clnAlg.h"

namespace cln {
	using boost::asio::ip::tcp;
	using namespace boost::asio;

	constexpr uint8_t DEF_N_CLN = 10;
	constexpr uint8_t MAX_ATTEMPTS = 3;

	class client {
	public:
		client(client&& cl) noexcept : thr_cln (std::move(cl.thr_cln)), ip_ (std::move(cl.ip_)), port_(cl.port_){}
		client(const std::string& ip, const uint16_t& port): ip_(ip), port_(port){ }
		void start() {
			thr_cln = std::thread(std::bind(&client::thr_func, this));
		}
		void stop() {
			std::unique_lock<std::mutex> lck(mtx);
			if (active != false) {
				active = false;
			}
			else {
				return;
			}
			
			lck.unlock();
			cv.notify_one();
			thr_cln.join();
		}
	private:
		bool error_handler(boost::system::error_code &ec) {
			if (!ec) return true;
			static uint8_t attempts{ MAX_ATTEMPTS };
			log_write->error("client error code:{} error message:{}", ec.value(), ec.message());
			--attempts;
			if (!attempts) {
				std::lock_guard<std::mutex> lck(mtx);
				return active = false;
			}
			return true;
		}

		void thr_func(){
			log_write->info("client start");
			active = true;
			ip::tcp::socket sock(io_context);
			ip::tcp::endpoint ep(ip::address::from_string(ip_), port_);
			boost::system::error_code ec;
			sock.connect(ep,ec);
			do {
				if (ec) {
					log_write->info("client connect error code:{} erro message:{}", ec.value(), ec.message());
					break;
				}
				for (;;) {
					uint32_t number_to_send = calg::random(0, 1023);
					log_write->info("client send number:{}",  number_to_send);
					size_t n_bytes_write = boost::asio::write(sock, boost::asio::buffer(&number_to_send, sizeof(number_to_send)),ec);
					if (!error_handler(ec)) {
						break;
					}
					uint64_t number_from_read{};
					boost::asio::read(sock, boost::asio::buffer(&number_from_read, sizeof(number_from_read)), ec);
					if (!error_handler(ec)) {
						break;
					}
					log_write->info("client read number:{}", number_from_read);
					std::unique_lock<std::mutex> lck(mtx);
					if (cv.wait_for(lck, std::chrono::milliseconds(1), [this] {return !active; })) {
						log_write->info("client calling stop for thread end");
						break;
					}
					lck.unlock();
				}
			} while (false);
			active = false;
			log_write->info("client end");
		}

	private:
		std::thread thr_cln;
		std::string ip_{};
		uint16_t port_{};
		std::mutex mtx;
		std::condition_variable cv;
		bool active{ false };
		boost::asio::io_context io_context;
	};

	class client_mgr {
		client_mgr() = default;
	public:
		client_mgr(const client_mgr&) = delete;
		client_mgr(client_mgr&&) = delete;
		client_mgr& operator=(const client_mgr&) = delete;
		client_mgr& operator= (client_mgr&&) = delete;

		static client_mgr& instance() {
			static client_mgr cmgr;
			return cmgr;
		}
		void start(const std::string& ip, const uint16_t& port) {
			log_write->info("client manager start ip:{} port:{}", ip, port);
			cln = std::make_unique<client>(std::forward<client>(client(ip, port)));
			cln->start();
		}
		void stop() {
			log_write->info("clients manager started waiting end");
			cln->stop();
			log_write->info("clients manager stop waiting end");
		}
	private:
		std::unique_ptr<client> cln;
	};
}
#define CLIENT cln::client_mgr::instance()