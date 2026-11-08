#pragma once

#include "Memory.hpp"

#include <cstddef>
#include <string>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

std::size_t loadProgram(const std::string& path, Memory& memory) {
    std::ifstream file(path, std::ios::binary);

    if (!file)
        throw std::runtime_error("não foi possivel abrir o programa: " + path);

    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
    };

    if (file.bad())
        throw std::runtime_error("erro ao ler o programa: " + path);

    const auto words = bytes.size() / Memory::wordBytes;

    if (words > memory.sizeWords())
        throw std::runtime_error("programa excede a memória principal");

    memory.reset();

    for (std::size_t i = 0; i < words; ++i) {
        const std::size_t offset = i * Memory::wordBytes;
        const Word value = Word(bytes[offset]) |
                           (Word(bytes[offset + 1]) << 8) |
                           (Word(bytes[offset + 2]) << 16) |
                           (Word(bytes[offset + 3]) << 24);

        memory.write(i, value);
    }

    return words;
}
