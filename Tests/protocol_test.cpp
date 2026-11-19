#include <gtest/gtest.h>

#include <bit>
#include <cstdint>

#include "cse/protocol.h"

namespace cse::test {

TEST(Protocol, EncodesRequestAsFourBytes) {
    uint8_t buffer[protocol::REQUEST_SIZE]{};
    EXPECT_TRUE(protocol::encode_request(1, buffer, sizeof(buffer)));
    if constexpr (std::endian::native == std::endian::little) {
        EXPECT_EQ(buffer[0], 1);
        EXPECT_EQ(buffer[1], 0);
        EXPECT_EQ(buffer[2], 0);
        EXPECT_EQ(buffer[3], 0);
    } else {
        EXPECT_EQ(buffer[0], 0);
        EXPECT_EQ(buffer[3], 1);
    }
}

TEST(Protocol, RequestRoundTrip) {
    constexpr uint32_t number = 0x10230405;
    uint8_t buffer[protocol::REQUEST_SIZE]{};
    ASSERT_TRUE(protocol::encode_request(number, buffer, sizeof(buffer)));
    uint32_t decoded = 0;
    EXPECT_TRUE(protocol::decode_request(buffer, protocol::REQUEST_SIZE, decoded));
    EXPECT_EQ(decoded, number);
}

TEST(Protocol, RequestRejectsWrongSize) {
    const uint8_t buffer[protocol::RESPONSE_SIZE]{};
    uint32_t decoded = 0;
    EXPECT_FALSE(protocol::decode_request(buffer, protocol::REQUEST_SIZE - 1, decoded));
    EXPECT_FALSE(protocol::decode_request(buffer, protocol::REQUEST_SIZE + 1, decoded));
}

TEST(Protocol, RequestRejectsNullBuffer) {
    uint32_t decoded = 0;
    EXPECT_FALSE(protocol::decode_request(nullptr, protocol::REQUEST_SIZE, decoded));
    EXPECT_FALSE(protocol::encode_request(1, nullptr, protocol::REQUEST_SIZE));
    uint8_t small[protocol::REQUEST_SIZE]{};
    EXPECT_FALSE(protocol::encode_request(1, small, protocol::REQUEST_SIZE - 1));
}

TEST(Protocol, EncodesResponseAsEightBytes) {
    uint8_t buffer[protocol::RESPONSE_SIZE]{};
    EXPECT_TRUE(protocol::encode_response(37, buffer, sizeof(buffer)));
    if constexpr (std::endian::native == std::endian::little) {
        EXPECT_EQ(buffer[0], 37);
        EXPECT_EQ(buffer[1], 0);
        EXPECT_EQ(buffer[7], 0);
    } else {
        EXPECT_EQ(buffer[0], 0);
        EXPECT_EQ(buffer[7], 37);
    }
}

TEST(Protocol, ResponseRoundTrip) {
    constexpr uint64_t mean = 1046529;
    uint8_t buffer[protocol::RESPONSE_SIZE]{};
    ASSERT_TRUE(protocol::encode_response(mean, buffer, sizeof(buffer)));
    uint64_t decoded = 0;
    EXPECT_TRUE(protocol::decode_response(buffer, protocol::RESPONSE_SIZE, decoded));
    EXPECT_EQ(decoded, mean);
}

TEST(Protocol, ResponseRejectsWrongSize) {
    const uint8_t buffer[protocol::RESPONSE_SIZE]{};
    uint64_t decoded = 0;
    EXPECT_FALSE(protocol::decode_response(buffer, protocol::RESPONSE_SIZE - 1, decoded));
    EXPECT_FALSE(protocol::decode_response(buffer, protocol::RESPONSE_SIZE + 1, decoded));
}

TEST(Protocol, ResponseRejectsNullBuffer) {
    uint64_t decoded = 0;
    EXPECT_FALSE(protocol::decode_response(nullptr, protocol::RESPONSE_SIZE, decoded));
    EXPECT_FALSE(protocol::encode_response(37, nullptr, protocol::RESPONSE_SIZE));
    uint8_t small[protocol::RESPONSE_SIZE]{};
    EXPECT_FALSE(protocol::encode_response(37, small, protocol::RESPONSE_SIZE - 1));
}

}  // namespace cse::test
