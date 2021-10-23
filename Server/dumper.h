#pragma once
#include <unordered_set>
#include <iterator>
#include <thread>
#include <mutex>
#include <fstream>
#include "SrvAlg.h"
namespace srv {

    constexpr int DUMP_TIMEOUT = 5;
    
    class dump_writer final {
        struct data_block {
            data_block() = default;
            data_block(const data_block& db) {
                number_connection = db.number_connection;
                nums = db.nums;
            }
            data_block(uint32_t number_connection_, std::unordered_set<uint64_t>& numbers) : number_connection(number_connection_) {
                nums.reserve(numbers.size());
                std::copy(numbers.begin(), numbers.end(), std::back_inserter(nums));
            }
            data_block(data_block&& db) noexcept {
                number_connection = db.number_connection;
                nums = std::move(db.nums);
            }
            data_block& operator= (data_block&& db) noexcept{
                number_connection = db.number_connection;
                nums = std::move(db.nums);
                return *this;
            }
            uint32_t number_connection{};
            std::vector<uint64_t> nums;
        };

        struct data_block_hash {
            size_t operator () (const data_block& db) const {
                return std::hash<uint32_t>()(db.number_connection);
            }
        };

        struct data_block_equal {
            bool operator()(const data_block& _left, const data_block& _right) const {
                return _left.number_connection == _right.number_connection;
            }
        };
    public:
        dump_writer() {
            thr_dump = std::thread(std::bind(&dump_writer::dump_func, this));
        }
        ~dump_writer() {
            {
                std::unique_lock<std::mutex> lock(this->mutex_);
                active = false;
            }
            cv_.notify_one();
            thr_dump.join();
        }
        void to_dump(uint32_t number_connection, std::unordered_set<uint64_t>& numbers) {
            log_write->info("dump_writer::to_dump number connection:{} size set:{}", number_connection, numbers.size());
            pq.push(data_block(number_connection, numbers));
            {    std::unique_lock<std::mutex> lock(mutex_);    }
            cv_.notify_one();
        }
    private:
        void dump_func() {
            active = true;
            auto save_dumps = [&] {
                std::unordered_set<data_block, data_block_hash, data_block_equal> blocks;
                data_block db;
                while (pq.pop(db)) {
                    blocks.emplace(db);
                }
                for (auto &blk : blocks) {
                    save_dump_to_file(blk);
                }
                
            };
            for (;;) {
                save_dumps();
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    if (cv_.wait_for(lock, std::chrono::seconds(DUMP_TIMEOUT), [this] { return !active; })) {
                        save_dumps();
                        break;
                    }
                }
            }
        }

        bool exists_file(const std::string& name) {
            struct stat buffer;
            return (stat(name.c_str(), &buffer) == 0);
        }

        void save_dump_to_file(const data_block& db) {
            std::string name_file = std::to_string(db.number_connection) + ".dmp";
            if (exists_file(name_file)) {
                log_write->info("save_dump_to_file number connection:{} remove file:{}", db.number_connection, name_file.c_str());
                remove(name_file.c_str());
            }

            std::ofstream ofs(name_file, std::ofstream::binary);
            log_write->info("save_dump_to_file number connection:{} open file for write:{} count digits:{}", db.number_connection, name_file.c_str(), db.nums.size());
            for (auto num : db.nums) {
                ofs << num;
            }
            ofs.close();
            log_write->info("save_dump_to_file number connection:{} close written file:{}", db.number_connection, name_file.c_str());
        }
    private:
        std::thread thr_dump{};
        salg::parallel_queue<data_block> pq{};
        std::mutex mutex_;
        std::condition_variable cv_;
        bool active{ false };
    };

}