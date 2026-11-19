#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <set>
#include <thread>
#include <vector>

#include "cse/mean_of_squares.h"
#include "cse/numbers.h"

namespace cse::test {
namespace {

constexpr int THREADS = 8;
constexpr int ADDS_PER_THREAD = 1000;
constexpr uint32_t VALUE_SPAN = 1024;

}  // namespace

TEST(LockedRepository, EightThreadsLoseNothing) {
    locked_repository repo;
    std::vector<std::thread> workers;
    workers.reserve(THREADS);
    for (int thread_index = 0; thread_index < THREADS; ++thread_index) {
        workers.emplace_back([&repo, thread_index] {
            for (int i = 0; i < ADDS_PER_THREAD; ++i) {
                repo.add(static_cast<uint32_t>((thread_index * 128 + i) % VALUE_SPAN));
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    std::set<uint32_t> expected;
    for (int thread_index = 0; thread_index < THREADS; ++thread_index) {
        for (int i = 0; i < ADDS_PER_THREAD; ++i) {
            expected.insert(static_cast<uint32_t>((thread_index * 128 + i) % VALUE_SPAN));
        }
    }
    EXPECT_EQ(repo.size(), expected.size());

    uint64_t expected_sum = 0;
    for (const uint32_t number : expected) {
        expected_sum += uint64_t(number) * number;
    }
    EXPECT_EQ(repo.mean_of_squares().value_or(0), expected_sum / expected.size());
    EXPECT_EQ(repo.snapshot(), std::vector<uint32_t>(expected.begin(), expected.end()));
}

TEST(LockedRepository, EmptyRepositoryHasNoMean) {
    locked_repository repo;
    EXPECT_EQ(repo.size(), 0u);
    EXPECT_FALSE(repo.mean_of_squares().has_value());
}

TEST(LockedRepository, AddAndSnapshotConcurrently) {
    locked_repository repo;
    std::atomic_bool done{false};
    std::thread writer([&repo, &done] {
        for (uint32_t number = 0; number < VALUE_SPAN; ++number) {
            repo.add(number);
        }
        done = true;
    });
    std::size_t observed = 0;
    while (!done) {
        observed = repo.snapshot().size();
        std::this_thread::yield();
    }
    writer.join();
    EXPECT_LE(observed, repo.size());
    EXPECT_EQ(repo.size(), VALUE_SPAN);
}

}  // namespace cse::test
