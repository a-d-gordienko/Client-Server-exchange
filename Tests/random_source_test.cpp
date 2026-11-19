#include <gtest/gtest.h>

#include <cstdint>

#include "cse/random_source.h"

namespace cse::test {
namespace {

constexpr uint32_t SEED = 42;
constexpr int SAMPLES = 100;
constexpr int MANY_SAMPLES = 10000;

}  // namespace

TEST(RandomSource, SameSeedSameSequence) {
    MtRandomSource first(SEED);
    MtRandomSource second(SEED);
    for (int i = 0; i < SAMPLES; ++i) {
        EXPECT_EQ(first.next(), second.next());
    }
}

TEST(RandomSource, ValuesStayInRange) {
    MtRandomSource source(SEED);
    for (int i = 0; i < MANY_SAMPLES; ++i) {
        const uint32_t value = source.next();
        EXPECT_GE(value, MIN_NUMBER);
        EXPECT_LE(value, MAX_NUMBER);
    }
}

}  // namespace cse::test
