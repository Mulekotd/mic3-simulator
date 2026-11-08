#include "ALU.hpp"
#include "AMUX.hpp"
#include "Clock.hpp"
#include "ControlStore.hpp"
#include "MMUX.hpp"
#include "Memory.hpp"
#include "ProgramLoader.hpp"
#include "RAM.hpp"
#include "Shifter.hpp"
#include "registers/CPURegisters.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::cerr << "Uso: mic3_simulator programa.bin [--cycles N]\n";
        return 1;
    }

    std::uint64_t requestedCycles = 1;

    if (argc == 4) {
        if (std::string_view(argv[2]) != "--cycles") {
            std::cerr << "Uso: mic3_simulator programa.bin [--cycles N]\n";
            return 1;
        }

        const std::string_view number(argv[3]);
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), requestedCycles);

        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) {
            std::cerr << "N deve ser um inteiro nao negativo\n";
            return 1;
        }
    }

    try {
        CPURegisters cpu;
        RAM ram;
        Memory& memory = ram;
        ControlStore controlMemory;
        Clock clock;

        const auto loadedWords = loadProgram(argv[1], memory);

        for (std::uint64_t i = 0; i < requestedCycles; ++i) {
            // Circuitos combinacionais usam o estado antes da borda.
            // Micro-operacao temporaria: PC <- PC + 1.
            const auto a = cpu.bank.read(RegisterBank::PC);
            const auto b = cpu.bank.read(RegisterBank::ONE);
            const auto alu = ALU::execute(AMUX::select(a, cpu.mbr.read(), 0), b, 0b00);
            const auto nextPC = Shifter::execute(alu.value, 0b00);
            const auto nextMPC = MMUX::select(cpu.mpc, 0, 0b00, alu.n, alu.z);
            const auto nextMIR = controlMemory.read(cpu.mpc);

            clock.tick([&] {
                cpu.a.write(a);
                cpu.b.write(b);
                cpu.bank.write(RegisterBank::PC, nextPC);
                cpu.mir.write(nextMIR);
                cpu.mpc = nextMPC;
                cpu.n = alu.n;
                cpu.z = alu.z;
            });
        }

        std::cout << "loaded_words=" << loadedWords
                  << " cycles=" << clock.cycles()
                  << " PC=" << cpu.bank.read(RegisterBank::PC)
                  << " MPC=" << static_cast<unsigned>(cpu.mpc)
                  << " RAM_words=" << memory.sizeWords() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
