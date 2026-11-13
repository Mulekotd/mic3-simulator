#pragma once

#include "Memory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

struct StackSnapshot {
    std::uint32_t sp = 0;
    std::uint16_t depth = 0;
    std::vector<Word> words;
};

// Leitura do estado arquitetural: a pilha continua armazenada na RAM comum.
class StackInspector {
public:
    static constexpr std::uint32_t emptySp = 0xFFC;
    static constexpr std::size_t maxVisibleWords = 32;

    static StackSnapshot capture(const Memory& memory, std::uint32_t sp) {
        StackSnapshot snapshot;
        snapshot.sp = sp;

        if (sp > emptySp || sp >= memory.sizeWords())
            return snapshot;

        snapshot.depth = static_cast<std::uint16_t>(emptySp - sp);
        const std::size_t count = std::min<std::size_t>(snapshot.depth, maxVisibleWords);
        snapshot.words.reserve(count);

        for (std::size_t offset = 0; offset < count; ++offset)
            snapshot.words.push_back(memory.read(sp + offset));

        return snapshot;
    }
};
