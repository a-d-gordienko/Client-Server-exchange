#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace cse {

class IStopSource {
public:
    virtual ~IStopSource() = default;
    // Observer: возвращает идентификатор подписки (0 — источник уже остановлен).
    virtual std::uint64_t subscribe(std::function<void()> handler) = 0;
    virtual void unsubscribe(std::uint64_t id) = 0;
    virtual void request_stop() = 0;  // идемпотентно, один broadcast
    virtual bool stop_requested() const = 0;
    virtual void wait() const = 0;    // блокирует до request_stop
};

class stop_source final : public IStopSource {
public:
    std::uint64_t subscribe(std::function<void()> handler) override;
    void unsubscribe(std::uint64_t id) override;
    void request_stop() override;
    bool stop_requested() const override;
    void wait() const override;

    // Для тестов: сколько подписчиков ещё живо.
    std::size_t subscriber_count() const;

private:
    using entry = std::pair<std::uint64_t, std::function<void()>>;

    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;
    std::vector<entry> handlers_;
    std::uint64_t next_id_{1};
    bool stopped_{false};
};

namespace detail {

// notify_all обязан выполниться даже если обработчик бросит: иначе wait() повиснет.
struct notify_on_exit {
    std::condition_variable& cv;
    ~notify_on_exit() { cv.notify_all(); }
};

}  // namespace detail

inline std::uint64_t stop_source::subscribe(std::function<void()> handler) {
    {
        const std::lock_guard lock(mtx_);
        if (!stopped_) {
            const std::uint64_t id = next_id_++;
            handlers_.emplace_back(id, std::move(handler));
            return id;
        }
    }
    handler();
    return 0;
}

inline void stop_source::unsubscribe(std::uint64_t id) {
    const std::lock_guard lock(mtx_);
    for (auto it = handlers_.begin(); it != handlers_.end(); ++it) {
        if (it->first == id) {
            handlers_.erase(it);
            return;
        }
    }
}

inline void stop_source::request_stop() {
    std::vector<entry> handlers;
    {
        const std::lock_guard lock(mtx_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
        handlers.swap(handlers_);
    }
    const detail::notify_on_exit notify{cv_};
    for (const entry& handler : handlers) {
        try {
            handler.second();
        } catch (...) {
            // Один сбойный подписчик не должен мешать остановке остальных.
        }
    }
}

inline std::size_t stop_source::subscriber_count() const {
    const std::lock_guard lock(mtx_);
    return handlers_.size();
}

inline bool stop_source::stop_requested() const {
    const std::lock_guard lock(mtx_);
    return stopped_;
}

inline void stop_source::wait() const {
    std::unique_lock lock(mtx_);
    cv_.wait(lock, [this] { return stopped_; });
}

// Тестовый двойник: тот же Observer, но request_stop зовёт только тест.
class ManualStopSource final : public IStopSource {
public:
    std::uint64_t subscribe(std::function<void()> handler) override { return impl_.subscribe(std::move(handler)); }
    void unsubscribe(std::uint64_t id) override { impl_.unsubscribe(id); }
    void request_stop() override { impl_.request_stop(); }
    bool stop_requested() const override { return impl_.stop_requested(); }
    void wait() const override { impl_.wait(); }

    std::size_t subscriber_count() const { return impl_.subscriber_count(); }

private:
    stop_source impl_;
};

}  // namespace cse
