#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "cse/mean_of_squares.h"
#include "cse/numbers.h"

namespace cse::test {
namespace {

constexpr uint64_t SQUARE_OF_1023 = 1046529;

}  // namespace

TEST(MeanOfSquares, EmptyStoreHasNoValue) {
    const numbers_store store;
    EXPECT_FALSE(mean_of_squares(store).has_value());
}

TEST(MeanOfSquares, Zero) {
    numbers_store store;
    store.add(0);
    EXPECT_EQ(mean_of_squares(store).value_or(SQUARE_OF_1023), 0u);
}

TEST(MeanOfSquares, MaxValue) {
    numbers_store store;
    store.add(1023);
    EXPECT_EQ(mean_of_squares(store).value_or(0), SQUARE_OF_1023);
}

TEST(MeanOfSquares, FullRange) {
    numbers_store store;
    uint64_t expected_sum = 0;
    for (uint32_t number = 1; number <= 1023; ++number) {
        ASSERT_TRUE(store.add(number));
        expected_sum += uint64_t(number) * number;
    }
    EXPECT_EQ(mean_of_squares(store).value_or(0), expected_sum / store.size());
}

TEST(MeanOfSquares, DuplicateDoesNotChange) {
    numbers_store store;
    store.add(5);
    store.add(5);
    store.add(7);
    EXPECT_EQ(mean_of_squares(store).value_or(0), (25u + 49u) / 2u);
}

}  // namespace cse::test
