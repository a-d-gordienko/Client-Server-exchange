#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "cse/logger.h"

namespace cse {

// Пишет бинарный дамп (uint32 подряд) в отдельном потоке: сам тянет snapshot
// через cv-таймер, файл подменяется атомарно tmp -> rename.
class dump_writer {
public:
    dump_writer(std::filesystem::path file, std::chrono::milliseconds interval,
                std::function<std::vector<uint32_t>()> snapshot_fn, ILogger& log);
    ~dump_writer();

    dump_writer(const dump_writer&) = delete;
    dump_writer& operator=(const dump_writer&) = delete;

    void start();  // повторный start() после stop() снова запускает поток
    void stop();   // notify → финальный snapshot + запись → join

private:
    void run();
    void write_once();

    std::filesystem::path file_;
    std::filesystem::path tmp_;
    std::chrono::milliseconds interval_;
    std::function<std::vector<uint32_t>()> snapshot_fn_;
    ILogger& log_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stopping_{false};
    std::thread thread_;
};

}  // namespace cse
