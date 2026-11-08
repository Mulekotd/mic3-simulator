#pragma once

#include "Memory.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

class RAM : public Memory {
public:
    static constexpr std::size_t size = 4096; // 4096 * 4 bytes = 16 KiB
    static constexpr std::size_t sizeBytes = size * Memory::wordBytes;

    std::size_t sizeWords() const override;
    Word read(std::size_t address) const override;
    void write(std::size_t address, Word value) override;
    void reset() override;

private:
    std::array<Word, size> words_{};
};

inline std::size_t RAM::sizeWords() const { return size; }

inline Word RAM::read(std::size_t address) const {
    if (address >= size)
        throw std::out_of_range("endereco fora da RAM");

    return words_[address];
}

inline void RAM::write(std::size_t address, Word value) {
    if (address >= size)
        throw std::out_of_range("endereco fora da RAM");

    words_[address] = value;
}

inline void RAM::reset() { words_.fill(0); }
