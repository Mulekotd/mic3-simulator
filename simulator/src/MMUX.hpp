#pragma once

#include <cstdint>
#include <stdexcept>

class MMUX {
public:
    static std::uint8_t select(std::uint8_t mpc, std::uint8_t address, std::uint8_t cond, bool n, bool z);
};

inline std::uint8_t MMUX::select(std::uint8_t mpc, std::uint8_t address, std::uint8_t cond, bool n, bool z) {
    switch (cond) {
        case 0b00: break;
        case 0b01: if (n) return address; break;
        case 0b10: if (z) return address; break;
        case 0b11: return address;
        default: throw std::invalid_argument("a condição deve ter 2 bits");
    }

    return static_cast<std::uint8_t>(mpc + 1);
}
