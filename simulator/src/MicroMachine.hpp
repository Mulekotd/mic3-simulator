#pragma once

#include "ALU.hpp"
#include "AMUX.hpp"
#include "Clock.hpp"
#include "ControlStore.hpp"
#include "MMUX.hpp"
#include "Memory.hpp"
#include "Shifter.hpp"
#include "generated/Mic1Rom.hpp"
#include "registers/CPURegisters.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

// Retrato da microinstrução executada e dos efeitos observáveis no ciclo.
struct MicroCycleEvent {
    std::uint8_t mpc = 0;
    std::uint64_t word = 0;
    bool integerOutput = false;
    bool characterOutput = false;
    Word output = 0;
    bool halted = false;
};

class MicroMachine {
public:
    explicit MicroMachine(Memory& memory) : memory_(memory) {
        // A ROM simbólica já foi codificada em palavras de controle de 36 bits.
        for (std::size_t i = 0; i < mic1Rom.size(); ++i)
            control_.write(static_cast<std::uint8_t>(i), mic1Rom[i]);
    }

    MicroCycleEvent step();
    const CPURegisters& cpu() const { return cpu_; }
    std::uint64_t cycles() const { return clock_.cycles(); }
    std::uint64_t instructions() const { return instructions_; }
    bool halted() const { return halted_; }

private:
    // Uma leitura ou escrita da RAM leva dois ciclos de clock.
    struct Pending {
        bool active = false;
        bool read = false;
        Word address = 0;
        Word value = 0;
        unsigned remaining = 0;
    };

    Memory& memory_;
    CPURegisters cpu_;
    ControlStore control_;
    Clock clock_;
    Pending pending_;
    std::uint64_t instructions_ = 0;
    bool halted_ = false;

    void validateAddress(std::uint64_t address, bool write = false) const;
    static void validateInstruction(Word word);
    void advanceMemory(MicroCycleEvent& event);
    void validateMarSource(std::uint8_t mpc, Word raw) const;
    void executeMicro(MicroCycleEvent& event);
};

inline MicroCycleEvent MicroMachine::step() {
    if (halted_)
        throw std::logic_error("simulação já encerrada");

    MicroCycleEvent event;
    event.mpc = cpu_.mpc;
    event.word = control_.read(cpu_.mpc);

    // A resposta da memória chega antes da ULA consumir os registradores.
    clock_.tick([&] {
        advanceMemory(event);

        if (halted_)
            return;

        executeMicro(event);
    });

    event.halted = halted_;
    return event;
};

inline void MicroMachine::validateAddress(std::uint64_t address, bool write) const {
    if (address >= memory_.sizeWords() || (!write && address >= 0xFFD))
        throw std::out_of_range("endereço fora da memória principal");
}

inline void MicroMachine::validateInstruction(Word word) {
    // Rejeita bits reservados antes de executar uma palavra da ISA.
    const unsigned opcode = word >> 28;

    if (opcode < 15) {
        if (word & 0x0FFFF000u)
            throw std::runtime_error("instrução A com bits reservados não nulos");
    } else {
        const unsigned sub = (word >> 24) & 0xFu;

        if ((word & 0x00FFFF00u) || (sub & 1u) || (sub < 12 && (word & 0xFFu)))
            throw std::runtime_error("instrução B inválida");
    }
};

inline void MicroMachine::advanceMemory(MicroCycleEvent& event) {
    if (!pending_.active || --pending_.remaining != 0)
        return;

    if (pending_.read) {
        validateAddress(pending_.address);
        cpu_.mbr.write(memory_.read(pending_.address));
    } else {
        validateAddress(pending_.address, true);

        if (pending_.address == 0xFFD) {
            halted_ = true;
        } else if (pending_.address == 0xFFE) {
            event.characterOutput = true;
            event.output = pending_.value & 0xFFu;
        } else if (pending_.address == 0xFFF) {
            event.integerOutput = true;
            event.output = pending_.value;
        } else {
            memory_.write(pending_.address, pending_.value);
        }
    }

    pending_.active = false;
};

inline void MicroMachine::validateMarSource(std::uint8_t mpc, Word raw) const {
    // Alguns caminhos precisam validar o endereço completo antes do corte em 12 bits.
    if (mpc == 0 && raw >= 0xFFD)
        throw std::out_of_range("PC fora da memória de programa");

    if ((mpc == 32 || mpc == 34 || mpc == 37 || mpc == 39) && std::uint64_t{cpu_.bank.read(RegisterBank::SP)} + (cpu_.bank.read(RegisterBank::IR) & 0xFFFu) >= 0xFFD)
        throw std::out_of_range("endereço relativo fora da RAM");

    if ((mpc == 48 || mpc == 53 || mpc == 55 || mpc == 56 || mpc == 61 || mpc == 62 || mpc == 67) && raw >= 0xFFD)
        throw std::out_of_range("endereço de pilha ou indireto fora da RAM");

    if (mpc == 58 && raw >= memory_.sizeWords())
        throw std::out_of_range("endereço indireto fora da RAM");
};

inline void MicroMachine::executeMicro(MicroCycleEvent& event) {
    // Decodifica os campos da palavra de controle e percorre o caminho de dados.
    const std::uint64_t word = event.word;
    const std::uint8_t mpc = event.mpc;
    const std::uint8_t aIndex = (word >> 9) & 0x1Fu;
    const std::uint8_t bIndex = (word >> 14) & 0x1Fu;
    const std::uint8_t cIndex = (word >> 19) & 0x1Fu;
    const Word a = cpu_.bank.read(aIndex);
    const Word b = cpu_.bank.read(bIndex);
    const Word aluInput = AMUX::select(a, cpu_.mbr.read(), (word >> 35) & 1u);
    const auto result = ALU::execute(aluInput, b, (word >> 31) & 3u);
    const Word shifted = Shifter::execute(result.value, (word >> 29) & 3u);
    const bool rd = (word >> 26) & 1u;
    const bool wr = (word >> 25) & 1u;

    if (rd && wr)
        throw std::runtime_error("RD e WR simultâneos");

    const Word mar = ((word >> 27) & 1u) ? b & 0xFFFu : cpu_.mar.read();
    const Word mbr = ((word >> 28) & 1u) ? shifted : cpu_.mbr.read();

    if ((word >> 27) & 1u)
        validateMarSource(mpc, b);

    // RD/WR repetido no microprograma sustenta a mesma transação pendente.
    if (rd || wr) {
        if (!pending_.active) {
            pending_ = {true, rd, mar, mbr, 2};
        } else if (pending_.read != rd) {
            throw std::runtime_error("transações de memória sobrepostas");
        }
    }

    const std::uint8_t next = MMUX::select(
        mpc, (word >> 1) & 0xFFu, (word >> 33) & 3u, result.n, result.z);

    if (next == 0 && mpc != 0)
        ++instructions_;

    // As escritas de registradores tornam-se visíveis ao fim deste ciclo.
    cpu_.a.write(a);
    cpu_.b.write(b);

    if ((word >> 27) & 1u)
        cpu_.mar.write(mar);

    if ((word >> 28) & 1u)
        cpu_.mbr.write(shifted);

    if ((word >> 24) & 1u) {
        if (cIndex == RegisterBank::IR)
            validateInstruction(shifted);

        cpu_.bank.write(cIndex, shifted);
    }

    cpu_.mir.write(word);
    cpu_.mpc = next;
    cpu_.n = result.n;
    cpu_.z = result.z;
};
