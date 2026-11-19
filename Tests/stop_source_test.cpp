#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "cse/stop_source.h"

namespace cse::test {
namespace {

using namespace std::chrono_literals;

constexpr int CYCLES = 50;

// Ждёт в отдельном потоке stop.wait() и возвращает true, если он разблокировался за 1 с.
// При регрессе поток остаётся detach (пробу держит он сам), тест получает FAIL, а не hang.
bool wait_unblocks(const std::shared_ptr<stop_source>& stop) {
    struct probe {
        std::promise<void> ready;
        std::promise<void> woken;
    };
    const auto state = std::make_shared<probe>();
    std::future<void> ready = state->ready.get_future();
    std::future<void> woken = state->woken.get_future();

    std::thread waiter([stop, state] {
        state->ready.set_value();  // детерминированный handshake вместо sleep
        stop->wait();
        state->woken.set_value();
    });
    ready.wait();
    stop->request_stop();

    if (woken.wait_for(1s) != std::future_status::ready) {
        waiter.detach();
        return false;
    }
    waiter.join();
    return true;
}

}  // namespace

TEST(StopSource, HandlersCalledOnceOnRepeatedStop) {
    stop_source stop;
    int calls = 0;
    stop.subscribe([&calls] { ++calls; });
    stop.request_stop();
    stop.request_stop();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(stop.stop_requested());
}

TEST(StopSource, ThrowingHandlerDoesNotBlockOthers) {
    stop_source stop;
    int healthy_calls = 0;
    stop.subscribe([] { throw std::runtime_error("boom"); });
    stop.subscribe([&healthy_calls] { ++healthy_calls; });

    EXPECT_NO_THROW(stop.request_stop());
    EXPECT_EQ(healthy_calls, 1);
    EXPECT_TRUE(stop.stop_requested());
}

TEST(StopSource, ThrowingHandlerDoesNotHangWait) {
    const auto stop = std::make_shared<stop_source>();
    stop->subscribe([] { throw std::runtime_error("boom"); });
    EXPECT_TRUE(wait_unblocks(stop));
}

TEST(StopSource, WaitUnblocksOnRequestStop) {
    const auto stop = std::make_shared<stop_source>();
    EXPECT_TRUE(wait_unblocks(stop));
}

TEST(StopSource, SubscribeAfterStopRunsHandlerImmediately) {
    stop_source stop;
    stop.request_stop();
    int calls = 0;
    const std::uint64_t id = stop.subscribe([&calls] { ++calls; });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(id, 0u);  // подписки нет — отписываться не от чего
    EXPECT_EQ(stop.subscriber_count(), 0u);
}

TEST(StopSource, UnsubscribeKeepsSubscriberCountFlat) {
    stop_source stop;
    int calls = 0;
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        const std::uint64_t id = stop.subscribe([&calls] { ++calls; });
        ASSERT_NE(id, 0u);
        EXPECT_EQ(stop.subscriber_count(), 1u);
        stop.unsubscribe(id);
        EXPECT_EQ(stop.subscriber_count(), 0u);
    }
    stop.request_stop();
    EXPECT_EQ(calls, 0);  // отписавшиеся обработчики не зовутся
}

TEST(StopSource, ManualStopSourceBehavesLikeObserver) {
    ManualStopSource stop;
    int calls = 0;
    stop.subscribe([&calls] { ++calls; });
    EXPECT_FALSE(stop.stop_requested());
    stop.request_stop();
    EXPECT_TRUE(stop.stop_requested());
    EXPECT_EQ(calls, 1);
}

}  // namespace cse::test
