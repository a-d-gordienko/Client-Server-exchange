#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cse::protocol {

inline constexpr std::size_t REQUEST_SIZE = 4;   // uint32_t, host order
inline constexpr std::size_t RESPONSE_SIZE = 8;  // uint64_t, host order

inline bool decode_request(const uint8_t* data, std::size_t size, uint32_t& out) {
    if (data == nullptr || size != REQUEST_SIZE) {
        return false;
    }
    std::memcpy(&out, data, REQUEST_SIZE);
    return true;
}

inline bool encode_request(uint32_t number, uint8_t* out, std::size_t size) {
    if (out == nullptr || size < REQUEST_SIZE) {
        return false;
    }
    std::memcpy(out, &number, REQUEST_SIZE);
    return true;
}

inline bool decode_response(const uint8_t* data, std::size_t size, uint64_t& out) {
    if (data == nullptr || size != RESPONSE_SIZE) {
        return false;
    }
    std::memcpy(&out, data, RESPONSE_SIZE);
    return true;
}

inline bool encode_response(uint64_t mean, uint8_t* out, std::size_t size) {
    if (out == nullptr || size < RESPONSE_SIZE) {
        return false;
    }
    std::memcpy(out, &mean, RESPONSE_SIZE);
    return true;
}

}  // namespace cse::protocol
