#pragma once

#include <cstdint>
#include <stdexcept>

class ALU {
public:
    struct Result {
        std::uint32_t value;
        bool n;
        bool z;
    };

private:
    static Result makeResult(std::uint32_t value) {
        return {value, (value & (std::uint32_t{1} << 31)) != 0, value == 0};
    }

public:
    static Result execute(std::uint32_t a, std::uint32_t b, std::uint8_t aluOp) {
        switch (aluOp) {
            case 0b00: return makeResult(a + b);
            case 0b01: return makeResult(a & b);
            case 0b10: return makeResult(a);
            case 0b11: return makeResult(~a);
            default: throw std::invalid_argument("alu_op deve ter 2 bits");
        }
    }
};
