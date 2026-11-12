#pragma once

#include <cstdint>
#include <stdexcept>

class AMUX {
public:
    static std::uint32_t select(std::uint32_t aLatch, std::uint32_t mbr, std::uint8_t amux);
};

inline std::uint32_t AMUX::select(std::uint32_t aLatch, std::uint32_t mbr, std::uint8_t amux) {
    switch (amux) {
        case 0: return aLatch;
        case 1: return mbr;
        default: throw std::invalid_argument("amux deve ter 1 bit");
    }
}
