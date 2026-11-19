#include <gtest/gtest.h>

#include <vector>

#include "cse/numbers.h"

namespace cse::test {

TEST(NumbersStore, EmptyStore) {
    const numbers_store store;
    EXPECT_EQ(store.size(), 0u);
    EXPECT_EQ(store.sum_of_squares(), 0u);
    EXPECT_TRUE(store.snapshot().empty());
}

TEST(NumbersStore, DuplicateIgnored) {
    numbers_store store;
    EXPECT_TRUE(store.add(5));
    EXPECT_EQ(store.sum_of_squares(), 25u);
    EXPECT_FALSE(store.add(5));
    EXPECT_EQ(store.sum_of_squares(), 25u);
    EXPECT_EQ(store.size(), 1u);
}

TEST(NumbersStore, Boundaries) {
    numbers_store store;
    EXPECT_TRUE(store.add(0));
    EXPECT_TRUE(store.add(1023));
    EXPECT_EQ(store.size(), 2u);
    EXPECT_EQ(store.sum_of_squares(), 1023u * 1023u);
}

TEST(NumbersStore, RejectsOutOfRange) {
    numbers_store store;
    EXPECT_FALSE(store.add(MAX_NUMBER + 1));
    EXPECT_FALSE(store.add(1000000));
    EXPECT_EQ(store.size(), 0u);
    EXPECT_EQ(store.sum_of_squares(), 0u);
    EXPECT_TRUE(store.snapshot().empty());
}

TEST(NumbersStore, SnapshotIsAscending) {
    numbers_store store;
    store.add(7);
    store.add(5);
    store.add(0);
    EXPECT_EQ(store.snapshot(), (std::vector<uint32_t>{0, 5, 7}));
}

}  // namespace cse::test
