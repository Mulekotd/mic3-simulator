#include "MicroMachine.hpp"
#include "io/BinaryImage.hpp"
#include "RAM.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: mic3_simulator programa.bin [--cycles N] [--trace]\n";
        return 1;
    }

    const char* program = argv[1];
    std::uint64_t requestedCycles = 1;
    bool trace = false;
    bool cyclesSeen = false;

    // Analisa o limite de ciclos e a opção de rastreamento antes de carregar a RAM.
    for (int i = 2; i < argc; ++i) {
        const std::string_view option(argv[i]);

        if (option == "--trace" && !trace) {
            trace = true;
        } else if (option == "--cycles" && !cyclesSeen && i + 1 < argc) {
            const std::string_view number(argv[++i]);
            const auto parsed = std::from_chars(number.data(), number.data() + number.size(),
                                                requestedCycles);

            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) {
                std::cerr << "N deve ser um inteiro nao negativo\n";
                return 1;
            }

            cyclesSeen = true;
        } else {
            std::cerr << "Uso: mic3_simulator programa.bin [--cycles N] [--trace]\n";
            return 1;
        }
    }

    try {
        RAM ram;
        const auto loadedWords = loadBinaryImage(program, ram);
        MicroMachine machine(ram);
        bool characterLineOpen = false;

        // Cada chamada executa uma microinstrução e conclui transações pendentes.
        for (std::uint64_t i = 0; i < requestedCycles && !machine.halted(); ++i) {
            const auto event = machine.step();
            const auto& cpu = machine.cpu();

            if (event.integerOutput) {
                if (characterLineOpen) {
                    std::cout << '\n';
                    characterLineOpen = false;
                }

                // A saída numérica interpreta os mesmos 32 bits como inteiro com sinal.
                const std::int64_t signedValue = event.output <= 0x7FFFFFFFu ? event.output : std::int64_t{event.output} - 0x100000000LL;

                std::cout << "PROGRAM_OUT=" << signedValue << '\n';
            }

            if (event.characterOutput) {
                std::cout << static_cast<char>(event.output);
                characterLineOpen = true;
            }

            if (trace) {
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
        }

        if (characterLineOpen)
            std::cout << '\n';

        if (machine.halted())
            std::cout << "HALTED cycles=" << machine.cycles()
                      << " instructions=" << machine.instructions() << '\n';

        // Mantém um resumo mesmo quando o limite de ciclos encerra a execução.
        std::cout << "loaded_words=" << loadedWords
                  << " cycles=" << machine.cycles()
                  << " instructions=" << machine.instructions()
                  << " PC=" << machine.cpu().bank.read(RegisterBank::PC)
                  << " MPC=" << static_cast<unsigned>(machine.cpu().mpc)
                  << " RAM_words=" << ram.sizeWords()
                  << " halted=" << machine.halted() << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
