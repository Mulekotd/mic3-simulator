#include "Assembly.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

void usage() {
    std::cerr << "Uso: assembler program.asm [-o output.bin] [-l output.lst]\n";
}

bool sameFile(const std::filesystem::path& left, const std::filesystem::path& right) {
    // Impede que a saída sobrescreva a entrada ou a própria listagem.
    if (std::filesystem::absolute(left).lexically_normal() == std::filesystem::absolute(right).lexically_normal())
        return true;

    std::error_code error;

    return std::filesystem::equivalent(left, right, error) && !error;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 2;
    }

    std::filesystem::path input, output, listing;
    input = argv[1];

    bool outputSeen = false, listingSeen = false;

    for (int i = 2; i < argc; ++i) {
        const std::string_view option(argv[i]);

        if ((option != "-o" && option != "-l") || i + 1 >= argc) {
            usage();
            return 2;
        }

        if (option == "-o") {
            if (outputSeen) {
                usage();
                return 2;
            }

            output = argv[++i];
            outputSeen = true;
        } else {
            if (listingSeen) {
                usage();
                return 2;
            }

            listing = argv[++i];
            listingSeen = true;
        }
    }

    if (input.empty()) {
        usage();
        return 2;
    }

    if (!outputSeen)
        output = input.stem().string() + ".bin";

    if (!listingSeen) {
        listing = output;
        listing.replace_extension(".lst");
    }

    if (output.empty() || listing.empty()) {
        usage();
        return 2;
    }

    try {
        if (sameFile(input, output) || sameFile(input, listing) || sameFile(output, listing)) {
            std::cerr << "entrada, binário e listagem devem usar arquivos diferentes\n";
            return 2;
        }

        const auto program = assemble(input);
        writeProgram(program, output, listing);

        std::cout << "assembled_words=" << program.words.size() << " output=" << output.string() << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
