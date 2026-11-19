#pragma once

#include <charconv>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace cse {

inline constexpr uint16_t DEFAULT_PORT = 64000;
inline constexpr std::chrono::seconds DEFAULT_DUMP_INTERVAL{5};

struct options {
    std::string host{"127.0.0.1"};          // клиент
    uint16_t port{DEFAULT_PORT};            // сервер и клиент
    std::chrono::seconds dump_interval{DEFAULT_DUMP_INTERVAL};  // сервер

    // argv[0] пропускается; неизвестный флаг, отсутствующее или нечисловое
    // значение, порт вне uint16 — nullopt.
    static std::optional<options> parse(int argc, char* argv[]);
};

namespace detail {

inline bool parse_uint(std::string_view text, uint32_t& out) {
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc{} && result.ptr == end;
}

}  // namespace detail

inline std::optional<options> options::parse(int argc, char* argv[]) {
    options opts;
    for (int i = 1; i < argc; i += 2) {
        const std::string_view flag = argv[i];
        if (i + 1 >= argc) {
            return std::nullopt;
        }
        const std::string_view value = argv[i + 1];

        if (flag == "--host") {
            if (value.empty()) {
                return std::nullopt;
            }
            opts.host = std::string(value);
        } else if (flag == "--port") {
            uint32_t port = 0;
            // 0 — эфемерный порт: сервер занял бы случайный и клиент его не угадает.
            if (!detail::parse_uint(value, port) || port == 0 || port > UINT16_MAX) {
                return std::nullopt;
            }
            opts.port = static_cast<uint16_t>(port);
        } else if (flag == "--dump-interval") {
            uint32_t seconds = 0;
            // 0 секунд — tight-loop дампера.
            if (!detail::parse_uint(value, seconds) || seconds == 0) {
                return std::nullopt;
            }
            opts.dump_interval = std::chrono::seconds{seconds};
        } else {
            return std::nullopt;
        }
    }
    return opts;
}

}  // namespace cse
