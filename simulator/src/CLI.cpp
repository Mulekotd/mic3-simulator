#include "CLI.hpp"
#include "MicroMachine.hpp"
#include "RAM.hpp"
#include "io/BinaryImage.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

struct Options {
    const char* program = nullptr;
    std::optional<std::uint64_t> cycles;
    bool trace = false;
};

bool parseOptions(int argc, char* argv[], Options& options) {
    if (argc < 2) {
        std::cerr << "Uso: mic1-simulator program.bin [--cycles N] [--trace]\n";
        return false;
    }

    options.program = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string_view option(argv[i]);

        if (option == "--trace" && !options.trace) {
            options.trace = true;
        } else if (option == "--cycles" && !options.cycles && i + 1 < argc) {
            const std::string_view number(argv[++i]);
            std::uint64_t limit = 0;

            const auto parsed = std::from_chars(number.data(), number.data() + number.size(), limit);

            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) {
                std::cerr << "N deve ser um inteiro não negativo\n";
                return false;
            }

            options.cycles = limit;
        } else {
            std::cerr << "Uso: mic1-simulator program.bin [--cycles N] [--trace]\n";
            return false;
        }
    }

    return true;
}

void printOutput(const MicroCycleEvent& event, bool& characterLineOpen) {
    if (event.integerOutput) {
        if (characterLineOpen) {
            std::cout << '\n';
            characterLineOpen = false;
        }

        const std::int64_t signedValue = event.output <= 0x7FFFFFFFu
            ? event.output : std::int64_t{event.output} - 0x100000000LL;

        std::cout << "PROGRAM_OUT=" << signedValue << '\n';
    }

    if (event.characterOutput) {
        std::cout << static_cast<char>(event.output);
        characterLineOpen = true;
    }
}

void printTrace(const MicroMachine& machine, const MicroCycleEvent& event) {
    const auto& cpu = machine.cpu();

    std::cerr << "cycle=" << machine.cycles()
              << " micro=" << static_cast<unsigned>(event.mpc)
              << " word=" << event.word
              << " PC=" << cpu.bank.read(RegisterBank::PC)
              << " AC=" << cpu.bank.read(RegisterBank::AC)
              << " SP=" << cpu.bank.read(RegisterBank::SP)
              << " IR=" << cpu.bank.read(RegisterBank::IR)
              << " MAR=" << cpu.mar.read() << " MBR=" << cpu.mbr.read()
              << " MPC=" << static_cast<unsigned>(cpu.mpc)
              << " N=" << cpu.n << " Z=" << cpu.z << '\n';
}

void printSummary(const MicroMachine& machine, const RAM& ram, std::size_t loadedWords) {
    if (machine.halted())
        std::cout << "HALTED cycles=" << machine.cycles()
                  << " instructions=" << machine.instructions() << '\n';

    std::cout << "loaded_words=" << loadedWords
              << " cycles=" << machine.cycles()
              << " instructions=" << machine.instructions()
              << " PC=" << machine.cpu().bank.read(RegisterBank::PC)
              << " MPC=" << static_cast<unsigned>(machine.cpu().mpc)
              << " RAM_words=" << ram.sizeWords()
              << " halted=" << machine.halted() << '\n';
}

}  // namespace

int runCLI(int argc, char* argv[]) {
    Options options;

    if (!parseOptions(argc, argv, options))
        return 1;

    try {
        RAM ram;
        const auto loadedWords = loadBinaryImage(options.program, ram); // Carrega o programa binário na memória principal.

        MicroMachine machine(ram);
        bool characterLineOpen = false;

        while (!machine.halted() && (!options.cycles || machine.cycles() < *options.cycles)) {
            const auto event = machine.step();
            printOutput(event, characterLineOpen);

            if (options.trace)
                printTrace(machine, event);
        }

        if (characterLineOpen)
            std::cout << '\n';

        printSummary(machine, ram, loadedWords);

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
