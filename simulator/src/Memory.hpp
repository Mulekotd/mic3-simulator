#pragma once

#include <cstddef>
#include <cstdint>

using Word = std::uint32_t;

// Endereços são índices de palavras de 32 bits
class Memory {
public:
    static constexpr std::size_t wordBits = 32;
    static constexpr std::size_t wordBytes = sizeof(Word);

    virtual ~Memory() = default;
    virtual std::size_t sizeWords() const = 0;
    virtual Word read(std::size_t address) const = 0;
    virtual void write(std::size_t address, Word value) = 0;
    virtual void reset() = 0;
};
