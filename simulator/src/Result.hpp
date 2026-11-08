#pragma once

#include <cstdint>

struct Result {
    std::uint32_t value;
    bool n;
    bool z;
};

inline Result makeResult(std::uint32_t value) {
    return {
        value,
        (value & (std::uint32_t{1} << 31)) != 0,
        value == 0
    };
}
