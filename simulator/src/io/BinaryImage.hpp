#pragma once

#include "../Memory.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

inline std::size_t loadBinaryImage(const std::string& path, Memory& memory) {
    std::ifstream file(path, std::ios::binary);

    if (!file)
        throw std::runtime_error("não foi possível abrir o programa: " + path);

    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    if (file.bad())
        throw std::runtime_error("erro ao ler o programa: " + path);

    // Verifica a imagem inteira antes de alterar o conteúdo da memória.
    if (bytes.size() % Memory::wordBytes != 0)
        throw std::runtime_error("o tamanho do programa deve ser múltiplo de 4 bytes");

    const auto words = bytes.size() / Memory::wordBytes;

    if (words > memory.sizeWords())
        throw std::runtime_error("programa excede a memória principal");

    memory.reset();

    // O montador grava o byte menos significativo primeiro (little endian).
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
