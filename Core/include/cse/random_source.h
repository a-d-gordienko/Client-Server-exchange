#pragma once

#include <cstdint>
#include <random>

#include "cse/numbers.h"

namespace cse {

class IRandomSource {
public:
    virtual ~IRandomSource() = default;
    virtual uint32_t next() = 0;
};

class MtRandomSource final : public IRandomSource {
public:
    explicit MtRandomSource(uint32_t seed = std::random_device{}());
    uint32_t next() override;

private:
    std::mt19937 gen_;
    std::uniform_int_distribution<uint32_t> dist_{MIN_NUMBER, MAX_NUMBER};
};

inline MtRandomSource::MtRandomSource(uint32_t seed) : gen_(seed) {}

inline uint32_t MtRandomSource::next() {
    return dist_(gen_);
}

}  // namespace cse
