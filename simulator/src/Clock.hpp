#pragma once

#include <cstdint>
#include <utility>


class Clock {
public:
    template <typename OnRisingEdge>

    void tick(OnRisingEdge&& onRisingEdge) {
        std::forward<OnRisingEdge>(onRisingEdge)();
        ++cycles_;
    };

    std::uint64_t cycles() const { return cycles_; };
    void reset() { cycles_ = 0; };

private:
    std::uint64_t cycles_ = 0;
};
