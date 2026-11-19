#pragma once

#include <cstdint>
#include <mutex>
#include <optional>

#include "cse/numbers.h"

namespace cse {

// Среднее арифметическое квадратов; пустое хранилище значения не имеет.
inline std::optional<uint64_t> mean_of_squares(const numbers_store& store) {
    if (store.size() == 0) {
        return std::nullopt;
    }
    return store.sum_of_squares() / store.size();
}

inline std::optional<uint64_t> locked_repository::mean_of_squares() const {
    const std::lock_guard lock(mtx_);
    return cse::mean_of_squares(store_);
}

}  // namespace cse
