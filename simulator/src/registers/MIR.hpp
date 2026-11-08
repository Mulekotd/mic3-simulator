#pragma once

#include <cstdint>
#include <stdexcept>

class MIR {
public:
    static constexpr std::uint64_t mask = (std::uint64_t{1} << 36) - 1;

    std::uint64_t read() const { return value_; }
    void write(std::uint64_t value, bool enable = true);
    void reset() { value_ = 0; }

private:
    std::uint64_t value_ = 0;
};

inline void MIR::write(std::uint64_t value, bool enable) {
    if (!enable) return;

    if (value > mask)
        throw std::out_of_range("valor excede 36 bits");

    value_ = value;
}
