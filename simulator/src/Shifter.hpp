#pragma once

#include <cstdint>
#include <stdexcept>

class Shifter {
public:
    static std::uint32_t execute(std::uint32_t value, std::uint8_t sh);
};

inline std::uint32_t Shifter::execute(std::uint32_t value, std::uint8_t sh) {
    switch (sh) {
        case 0b00: return value;       // Sem deslocamento (pass-through)
        case 0b01: return value >> 1;  // Shift right lógico (SRL): entra 0 no MSB
        case 0b10: return value << 1;  // Shift left lógico (SLL): entra 0 no LSB
        default: throw std::invalid_argument("sh inválido");
    }
}
