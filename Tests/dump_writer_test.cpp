#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "cse/dump_writer.h"
#include "cse/numbers.h"
#include "support/test_support.h"

namespace cse::test {
namespace {

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds DUMP_INTERVAL{100};
constexpr std::chrono::milliseconds WAIT_LIMIT{3000};

bool wait_until(const std::function<bool()>& predicate, std::chrono::milliseconds timeout = WAIT_LIMIT) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}

bool dump_has_size(const std::filesystem::path& file, std::size_t expected_count) {
    return std::filesystem::exists(file) && std::filesystem::file_size(file) == expected_count * sizeof(uint32_t);
}

}  // namespace

TEST(DumpWriter, WritesBinarySnapshotInAscendingOrder) {
    const std::filesystem::path file = make_temp_dir("dump_binary") / "numbers.dmp";
    locked_repository repo;  // как в проде: дампер и тест ходят в репозиторий под локом
    repo.add(7);
    repo.add(5);
    repo.add(0);
    RecordingLogger log;
    dump_writer writer(file, DUMP_INTERVAL, [&repo] { return repo.snapshot(); }, log);

    writer.start();
    ASSERT_TRUE(wait_until([&file] { return dump_has_size(file, 3); }));
    EXPECT_EQ(read_dump(file), (std::vector<uint32_t>{0, 5, 7}));
    writer.stop();
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".tmp"));
}

TEST(DumpWriter, OverwritesFileOnNextDump) {
    const std::filesystem::path file = make_temp_dir("dump_overwrite") / "numbers.dmp";
    locked_repository repo;
    repo.add(7);
    RecordingLogger log;
    dump_writer writer(file, DUMP_INTERVAL, [&repo] { return repo.snapshot(); }, log);

    writer.start();
    ASSERT_TRUE(wait_until([&file] { return dump_has_size(file, 1); }));

    repo.add(1023);
    ASSERT_TRUE(wait_until([&file] { return dump_has_size(file, 2); }));
    EXPECT_EQ(read_dump(file), (std::vector<uint32_t>{7, 1023}));
    writer.stop();
}

TEST(DumpWriter, StopDoesNotWaitForInterval) {
    const std::filesystem::path file = make_temp_dir("dump_stop") / "numbers.dmp";
    locked_repository repo;
    RecordingLogger log;
    dump_writer writer(file, std::chrono::seconds{10}, [&repo] { return repo.snapshot(); }, log);

    writer.start();
    const auto begin = std::chrono::steady_clock::now();
    writer.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - begin, std::chrono::seconds{1});
    EXPECT_TRUE(std::filesystem::exists(file));
}

TEST(DumpWriter, EmptySnapshotWritesEmptyFile) {
    const std::filesystem::path file = make_temp_dir("dump_empty") / "numbers.dmp";
    locked_repository repo;
    RecordingLogger log;
    dump_writer writer(file, DUMP_INTERVAL, [&repo] { return repo.snapshot(); }, log);

    writer.start();
    ASSERT_TRUE(wait_until([&file] { return dump_has_size(file, 0); }));
    EXPECT_EQ(std::filesystem::file_size(file), 0u);
    writer.stop();
}

TEST(DumpWriter, SurvivesThrowingSnapshotAndWritesNextTick) {
    const std::filesystem::path file = make_temp_dir("dump_throwing") / "numbers.dmp";
    RecordingLogger log;
    int calls = 0;  // трогает только поток дампера
    dump_writer writer(
        file, DUMP_INTERVAL,
        [&calls]() -> std::vector<uint32_t> {
            if (calls++ == 0) {
                throw std::runtime_error("snapshot failed");
            }
            return {1, 2};
        },
        log);

    writer.start();  // исключение из snapshot_fn_ не должно дойти до std::terminate
    ASSERT_TRUE(wait_until([&file] { return dump_has_size(file, 2); }));
    EXPECT_EQ(read_dump(file), (std::vector<uint32_t>{1, 2}));
    EXPECT_EQ(log.count_errors("snapshot failed"), 1u);
    writer.stop();
}

}  // namespace cse::test
