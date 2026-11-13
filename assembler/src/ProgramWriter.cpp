#include "Assembly.hpp"

#include <fstream>
#include <iomanip>
#include <stdexcept>

void writeProgram(const Program& program, const std::filesystem::path& output, const std::filesystem::path& listing) {
    std::ofstream binary(output, std::ios::binary);

    if (!binary)
        throw std::runtime_error("não foi possível criar: " + output.string());

    // O simulador lê cada palavra de 32 bits em ordem little endian.
    for (const auto word : program.words) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            binary.put(static_cast<char>((word >> shift) & 0xFFu));
    }

    if (!binary)
        throw std::runtime_error("erro ao gravar: " + output.string());

    binary.close();

    if (!binary)
        throw std::runtime_error("erro ao fechar: " + output.string());

    if (listing.empty())
        return;

    std::ofstream text(listing);

    if (!text)
        throw std::runtime_error("não foi possível criar: " + listing.string());

    text << "; endereço  palavra   linha  fonte\n";

    for (std::size_t i = 0; i < program.words.size(); ++i) {
        text << std::uppercase << std::hex << std::setfill('0')
             << std::setw(3) << i << "  " << std::setw(8) << program.words[i]
             << std::dec << std::setfill(' ') << "  " << std::setw(4) << program.lines[i]
             << "  " << program.sources[i] << '\n';
    }

    if (!text)
        throw std::runtime_error("erro ao gravar: " + listing.string());
}
