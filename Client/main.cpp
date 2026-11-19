#include <conio.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <thread>

#include <spdlog/common.h>

#include "cse/client.h"
#include "cse/logger.h"
#include "cse/options.h"
#include "cse/random_source.h"

namespace {

constexpr int KEY_ESCAPE = 27;
constexpr std::chrono::milliseconds POLL_INTERVAL{30};

}  // namespace

int main(int argc, char* argv[]) {
    const auto opts = cse::options::parse(argc, argv);
    if (!opts) {
        std::fprintf(stderr, "usage: cse_client [--host H] [--port N]\n");
        return 2;
    }

    std::shared_ptr<cse::ILogger> logger;
    try {
        logger = cse::SpdlogLogger::create("client.log");
    } catch (const spdlog::spdlog_ex& error) {
        std::fprintf(stderr, "cannot open client.log: %s\n", error.what());
        return 1;
    }

    cse::MtRandomSource random;
    cse::client_app app(*opts, random, *logger);

    std::promise<int> result;
    std::future<int> code = result.get_future();
    std::thread runner([&app, &result] { result.set_value(app.run()); });

    // Клиент завершается сам, как только run() вернул код; ESC — для ещё работающего.
    while (code.wait_for(POLL_INTERVAL) != std::future_status::ready) {
        if (_kbhit() && _getch() == KEY_ESCAPE) {
            app.request_stop();
            break;
        }
    }

    const int exit_code = code.get();  // после ESC run() возвращается быстро
    runner.join();
    return exit_code;
}
