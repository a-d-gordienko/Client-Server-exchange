#pragma once
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <string>
namespace _log{
    class Logger final {
    private:
        Logger() = default;
    public:
        Logger(const Logger& lg) = delete;
        Logger(Logger&& lg) = delete;
        Logger& operator=(const Logger& lg) = delete;
        Logger& operator= (Logger&& lg) = delete;
        static  Logger& instance() {
            static Logger logger_;
            return logger_;
        }
        void init(const std::string& filename_log) {
            spdlog::init_thread_pool(8192, 1);
            auto stdout_sink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
            stdout_sink->set_level(spdlog::level::info);
            stdout_sink->set_pattern("%D %H:%M:%S.%e %05t %05l %v");
            auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename_log, 1024 * 1024 * 10, 3);
            rotating_sink->set_level(spdlog::level::info);
            rotating_sink->set_pattern("%D %H:%M:%S.%e %05t %05l %v");
            std::vector<spdlog::sink_ptr> sinks{ stdout_sink, rotating_sink };
            logger = std::make_shared<spdlog::async_logger>("server_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
            logger->set_level(spdlog::level::info);
            spdlog::register_logger(logger);
            spdlog::flush_every(std::chrono::seconds(1));
        }
        std::shared_ptr<spdlog::logger> get() {
            return logger;
        }
    private:
        std::shared_ptr<spdlog::logger> logger;
    };
}
#define log_instance _log::Logger::instance()
#define log_write _log::Logger::instance().get()