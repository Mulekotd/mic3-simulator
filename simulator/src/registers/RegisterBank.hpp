#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

class RegisterBank {
public:
    enum Index : std::uint8_t {
        ZERO = 0, PC = 1, AC = 2, SP = 3, IR = 4, TIR = 5,
        ONE = 6, MONE = 7, AMASK = 8, SMASK = 9
    };

    RegisterBank() { reset(); }

    std::uint32_t read(std::uint8_t index) const;
    void write(std::uint8_t index, std::uint32_t value, bool enable = true);
    void reset();

private:
    std::array<std::uint32_t, 32> values_{};
};

inline std::uint32_t RegisterBank::read(std::uint8_t index) const {
    if (index >= values_.size())
        throw std::out_of_range("registrador inválido");

    return values_[index];
}

inline void RegisterBank::write(std::uint8_t index, std::uint32_t value, bool enable) {
    if (index >= values_.size())
        throw std::out_of_range("registrador inválido");

    if (!enable || index == ZERO || (index >= ONE && index <= SMASK))
        return;

    values_[index] = value;
}

inline void RegisterBank::reset() {
    values_.fill(0);             // Zera os 32 registradores
    values_[SP] = 0xFFC;         // Inicia a pilha no endereço de palavra 4092
    values_[ONE] = 1;            // Constante +1
    values_[MONE] = 0xFFFFFFFF;  // Constante -1
    values_[AMASK] = 0x00000FFF; // Máscara dos 12 bits do campo de endereço x
    values_[SMASK] = 0x000000FF; // Máscara dos 8 bits do campo y
}
