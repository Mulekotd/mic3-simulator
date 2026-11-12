#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

class ControlStore {
public:
    static constexpr std::uint64_t wordMask = (std::uint64_t{1} << 36) - 1;

    std::uint64_t read(std::uint8_t address) const { return words[address]; };
    void write(std::uint8_t address, std::uint64_t word);
    void reset() { words.fill(0); };

private:
    std::array<std::uint64_t, 256> words{};
};

inline void ControlStore::write(std::uint8_t address, std::uint64_t word) {
    if (word > wordMask)
        throw std::out_of_range("microinstrução excede 36 bits");

    words[address] = word;
}
