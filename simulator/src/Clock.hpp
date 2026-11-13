#pragma once

#include <cstdint>
#include <utility>

class Clock {
public:
    template <typename OnRisingEdge>

    void tick(OnRisingEdge&& onRisingEdge) {
        std::forward<OnRisingEdge>(onRisingEdge)();
        ++cycleCount;
    };

    std::uint64_t cycles() const { return cycleCount; };
    void reset() { cycleCount = 0; };

private:
    std::uint64_t cycleCount = 0;
};
