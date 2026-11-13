#pragma once

#include <cstdint>

class Latch {
public:
    std::uint32_t read() const { return storedValue; };
    void write(std::uint32_t value, bool enable = true) { if (enable) storedValue = value; };
    void reset() { storedValue = 0; };

private:
    std::uint32_t storedValue = 0;
};
