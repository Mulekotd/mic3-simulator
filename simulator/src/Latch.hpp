#pragma once

#include <cstdint>

class Latch {
public:
    std::uint32_t read() const { return value_; };
    void write(std::uint32_t value, bool enable = true);
    void reset() { value_ = 0; };

private:
    std::uint32_t value_ = 0;
};

inline void Latch::write(std::uint32_t value, bool enable) {
    if (enable)
        value_ = value;
}
