#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Program {
    std::vector<std::uint32_t> words; // A listagem acompanha cada palavra, inclusive os espaços criados por .org.
    std::vector<std::string> sources;
    std::vector<std::size_t> lines;
};

Program assemble(const std::filesystem::path& input);

void writeProgram(const Program& program, const std::filesystem::path& output,
                  const std::filesystem::path& listing);
