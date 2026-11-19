#include "cse/dump_writer.h"

#include <exception>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace cse {
namespace {

constexpr const char* kTmpSuffix = ".tmp";

}  // namespace

dump_writer::dump_writer(std::filesystem::path file, std::chrono::milliseconds interval,
                         std::function<std::vector<uint32_t>()> snapshot_fn, ILogger& log)
    : file_(std::move(file)),
      tmp_(file_.string() + kTmpSuffix),
      interval_(interval),
      snapshot_fn_(std::move(snapshot_fn)),
      log_(log) {}

dump_writer::~dump_writer() {
    stop();
}

void dump_writer::start() {
    if (thread_.joinable()) {
        return;
    }
    {
        const std::lock_guard lock(mtx_);
        stopping_ = false;  // после stop() объект снова работоспособен
    }
    thread_ = std::thread([this] { run(); });
}

void dump_writer::stop() {
    {
        const std::lock_guard lock(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void dump_writer::run() {
    std::unique_lock lock(mtx_);
    while (!cv_.wait_for(lock, interval_, [this] { return stopping_; })) {
        lock.unlock();
        write_once();
        lock.lock();
    }
    lock.unlock();
    write_once();
}

void dump_writer::write_once() {
    // Поток дампера не должен ронять процесс: сервер продолжает работу.
    try {
        const std::vector<uint32_t> numbers = snapshot_fn_();

        std::ofstream out(tmp_, std::ios::binary | std::ios::trunc);
        if (!out) {
            log_.error("dump: cannot open " + tmp_.string());
            return;
        }
        for (const uint32_t number : numbers) {
            out.write(reinterpret_cast<const char*>(&number), sizeof(number));
        }
        out.close();
        if (!out) {
            log_.error("dump: cannot write " + tmp_.string());
            return;
        }

        std::error_code ec;
        std::filesystem::rename(tmp_, file_, ec);
        if (ec == std::errc::file_exists) {
            // Цель существует и rename её не подменил — удаляем и пробуем ещё раз.
            std::error_code ignored;
            std::filesystem::remove(file_, ignored);
            ec.clear();
            std::filesystem::rename(tmp_, file_, ec);
        }
        if (ec) {
            // Прочие ошибки (нет прав, файл занят): прежний дамп не удаляем, но
            // недописанный tmp за собой убираем — иначе он копится рядом с дампом.
            std::error_code remove_error;
            std::filesystem::remove(tmp_, remove_error);
            log_.error("dump: cannot replace " + file_.string() + ": " + ec.message());
            return;
        }
        log_.debug("dump written: " + std::to_string(numbers.size()) + " numbers");
    } catch (const std::exception& error) {
        log_.error(std::string("dump: ") + error.what());
    }
}

}  // namespace cse
