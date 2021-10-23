#pragma once
#include <string>
#include <ctime>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include "connect.h"
#include <unordered_set>
#include <numeric>

#include <fstream>
namespace salg {
    
    using namespace con;

    template<class T>
    class parallel_queue {
    public:
        void push(const T& t) {
            const std::lock_guard<std::mutex> lock_mutex(mtx);
            wq->emplace(t);
        }
        bool pop(T& t) {
            auto _pop = [&]()->bool {
                if (!rq->empty()) {
                    t = std::move(rq->front());
                    rq->pop();
                    return true;
                }
                return false;
            };

            if (_pop()) {
                return true;
            }
            else {
                    {
                        const std::lock_guard<std::mutex> lock_mutex(mtx);
                        std::swap<std::queue<T>*>(rq, wq);
                    }
                return _pop();
            }
        }
    private:
        std::queue<T> write_q;
        std::queue<T> read_q;
        std::queue<T>* rq{ &read_q };
        std::queue<T>* wq{ &write_q };
        std::mutex mtx;
    };

    class storage_numbers final {
    public:
        void to_storage(uint32_t number_connection, uint32_t number) {
            storage[number_connection].insert(number*number);
        }
        
        bool get_arithmetic_mean(uint32_t number_connection, uint64_t& arithmetic_mean) {
            auto it = storage.find(number_connection);
            if (it != std::end(storage)) {
                arithmetic_mean =  (std::accumulate(std::begin(it->second), std::end(it->second), 0) / it->second.size());
                return true;
            }
            return false;
        }
        
        bool get_storage(const uint32_t number_connection, std::unordered_set<uint64_t>& nums) {
            auto it = storage.find(number_connection);
            if (it != std::end(storage)) {
                nums = it->second;
                return true;
            }
            return false;
        }

    private:
        std::unordered_map<uint32_t, std::unordered_set<uint64_t> > storage;
    };



}