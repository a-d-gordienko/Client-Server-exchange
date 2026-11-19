#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

namespace cse {

// Контракт ТЗ: сервер принимает числа 0..1023, среднее считается по квадратам.
inline constexpr uint32_t MIN_NUMBER = 0;
inline constexpr uint32_t MAX_NUMBER = 1023;

class numbers_store;

std::optional<uint64_t> mean_of_squares(const numbers_store& store);

// Только данные, без синхронизации: среднее по всем сохранённым числам считается
// из инкрементальной суммы квадратов, дубликат сумму не меняет.
class numbers_store {
public:
    // true — число новое; вне MIN_NUMBER..MAX_NUMBER или дубликат — false,
    // сумма квадратов не меняется.
    bool add(uint32_t number);
    std::size_t size() const;
    uint64_t sum_of_squares() const;
    std::vector<uint32_t> snapshot() const;

private:
    std::set<uint32_t> numbers_;
    uint64_t sum_of_squares_{0};
};

// Proxy над numbers_store: потокобезопасность отдельно от данных.
// mean_of_squares() определена в cse/mean_of_squares.h.
class locked_repository {
public:
    bool add(uint32_t number);
    std::vector<uint32_t> snapshot();
    std::optional<uint64_t> mean_of_squares() const;
    std::size_t size() const;

private:
    mutable std::mutex mtx_;
    numbers_store store_;
};

inline bool locked_repository::add(uint32_t number) {
    const std::lock_guard lock(mtx_);
    return store_.add(number);
}

inline std::vector<uint32_t> locked_repository::snapshot() {
    const std::lock_guard lock(mtx_);
    return store_.snapshot();
}

inline std::size_t locked_repository::size() const {
    const std::lock_guard lock(mtx_);
    return store_.size();
}

inline bool numbers_store::add(uint32_t number) {
    if (number > MAX_NUMBER) {
        return false;
    }
    const bool added = numbers_.insert(number).second;
    if (added) {
        sum_of_squares_ += uint64_t(number) * number;
    }
    return added;
}

inline std::size_t numbers_store::size() const {
    return numbers_.size();
}

inline uint64_t numbers_store::sum_of_squares() const {
    return sum_of_squares_;
}

inline std::vector<uint32_t> numbers_store::snapshot() const {
    return std::vector<uint32_t>(numbers_.begin(), numbers_.end());
}

}  // namespace cse
