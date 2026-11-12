#pragma once

#include <cstdint>
#include <stdexcept>

class MIR {
public:
    static constexpr std::uint64_t mask = (std::uint64_t{1} << 36) - 1;

    std::uint64_t read() const { return storedValue; }
    void write(std::uint64_t value, bool enable = true);
    void reset() { storedValue = 0; }

private:
    std::uint64_t storedValue = 0;
};

inline void MIR::write(std::uint64_t value, bool enable) {
    if (!enable) return;

    if (value > mask)
        throw std::out_of_range("valor excede 36 bits");

    storedValue = value;
}
