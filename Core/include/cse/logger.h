#pragma once

#include <memory>
#include <string>

#include <spdlog/common.h>

namespace spdlog {
class logger;
}

namespace cse {

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void info(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
    virtual void debug(const std::string& msg) = 0;
};

class SpdlogLogger final : public ILogger {
public:
    // Консоль + ротируемый файл. В реестр spdlog не регистрируется:
    // иначе два логгера в одном процессе конфликтуют по имени.
    static std::shared_ptr<ILogger> create(const std::string& file_path,
                                           spdlog::level::level_enum level = spdlog::level::info);

    // public ради std::make_shared; снаружи используйте create().
    explicit SpdlogLogger(std::shared_ptr<spdlog::logger> engine);

    void info(const std::string& msg) override;
    void error(const std::string& msg) override;
    void debug(const std::string& msg) override;

private:
    std::shared_ptr<spdlog::logger> engine_;
};

}  // namespace cse
