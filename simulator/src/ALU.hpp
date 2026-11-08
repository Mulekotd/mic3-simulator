#pragma once

#include "Result.hpp"
#include <cstdint>
#include <stdexcept>

class ALU {
public:
    static Result execute(std::uint32_t a, std::uint32_t b, std::uint8_t alu_op);
};

inline Result ALU::execute(std::uint32_t a, std::uint32_t b, std::uint8_t alu_op) {
    switch (alu_op) {
        case 0b00: return makeResult(a + b);
        case 0b01: return makeResult(a & b);
        case 0b10: return makeResult(a);
        case 0b11: return makeResult(~a);
        default: throw std::invalid_argument("alu_op deve ter 2 bits");
    }
}
