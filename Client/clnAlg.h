#pragma once
#include <random>

namespace calg {

    int random(uint32_t min, uint32_t max) {
        std::random_device rd;                              //Will be used to obtain a seed for the random number engine
        static std::mt19937 gen(rd());                      //Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(min, max);
        return distrib(gen);
    }
}
