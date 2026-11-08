#pragma once

#include <cstdint>
#include <stdexcept>

class Shifter {
public:
    static std::uint32_t execute(std::uint32_t value, std::uint8_t sh);
};

inline std::uint32_t Shifter::execute(std::uint32_t value, std::uint8_t sh) {
    switch (sh) {
        case 0b00: return value;
        case 0b01: return value >> 1;
        case 0b10: return value << 1;
        default: throw std::invalid_argument("sh inválido"); // Sinal de controle inválido
    }
}
