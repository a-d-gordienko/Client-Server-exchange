#include "cse/logger.h"

#include <cstddef>
#include <utility>
#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/wincolor_sink.h>
#include <spdlog/spdlog.h>

namespace cse {
namespace {

constexpr const char* kPattern = "%D %H:%M:%S.%e %05t %05l %v";
constexpr std::size_t kMaxFileSize = 10 * 1024 * 1024;
constexpr std::size_t kMaxFiles = 3;

void configure(const spdlog::sink_ptr& sink, spdlog::level::level_enum level) {
    sink->set_level(level);
    sink->set_pattern(kPattern);
}

}  // namespace

SpdlogLogger::SpdlogLogger(std::shared_ptr<spdlog::logger> engine) : engine_(std::move(engine)) {}

std::shared_ptr<ILogger> SpdlogLogger::create(const std::string& file_path, spdlog::level::level_enum level) {
    const auto console = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
    const auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, kMaxFileSize, kMaxFiles);
    configure(console, level);
    configure(file, level);

    std::vector<spdlog::sink_ptr> sinks{console, file};
    auto engine = std::make_shared<spdlog::logger>("cse", sinks.begin(), sinks.end());
    engine->set_level(level);
    // ESC — единственный штатный выход: warn+ сбрасываем сразу, иначе буфер теряется.
    engine->flush_on(spdlog::level::warn);
    return std::make_shared<SpdlogLogger>(std::move(engine));
}

void SpdlogLogger::info(const std::string& msg) {
    engine_->info("{}", msg);
}

void SpdlogLogger::error(const std::string& msg) {
    engine_->error("{}", msg);
}

void SpdlogLogger::debug(const std::string& msg) {
    engine_->debug("{}", msg);
}

}  // namespace cse
