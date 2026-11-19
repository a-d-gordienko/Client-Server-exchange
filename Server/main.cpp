#include <conio.h>

#include <cstdio>
#include <memory>

#include <boost/system/system_error.hpp>
#include <spdlog/common.h>

#include "cse/logger.h"
#include "cse/options.h"
#include "cse/server.h"

namespace {

constexpr int KEY_ESCAPE = 27;

}  // namespace

int main(int argc, char* argv[]) {
    const auto opts = cse::options::parse(argc, argv);
    if (!opts) {
        std::fprintf(stderr, "usage: cse_server [--port N] [--dump-interval SEC]\n");
        return 2;
    }

    std::shared_ptr<cse::ILogger> logger;
    try {
        logger = cse::SpdlogLogger::create("server.log");
    } catch (const spdlog::spdlog_ex& error) {
        std::fprintf(stderr, "cannot open server.log: %s\n", error.what());
        return 1;
    }

    try {
        cse::server_app app(opts->port, opts->dump_interval, "numbers.dmp", *logger);
        app.start();
        logger->info("start server");

        while (_getch() != KEY_ESCAPE) {
        }

        app.stop();
        logger->info("stop server");
    } catch (const boost::system::system_error& error) {
        std::fprintf(stderr, "cannot start server: %s\n", error.what());
        return 1;
    }
    return 0;
}
