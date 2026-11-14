#include "MicroMachine.hpp"

#include "ALU.hpp"
#include "AMUX.hpp"
#include "MMUX.hpp"
#include "Shifter.hpp"
#include "generated/Mic1Rom.hpp"

#include <cstddef>
#include <stdexcept>

namespace {

// Nomes dos campos de uma palavra de controle de 36 bits.
struct MicroFields {
    explicit MicroFields(std::uint64_t word)
        : a((word >> 9) & 0x1F), b((word >> 14) & 0x1F), c((word >> 19) & 0x1F),
          alu((word >> 31) & 0x03), shift((word >> 29) & 0x03),
          cond((word >> 33) & 0x03), address((word >> 1) & 0xFF),
          useMbr((word >> 35) & 1), writeMbr((word >> 28) & 1),
          writeMar((word >> 27) & 1), read((word >> 26) & 1),
          write((word >> 25) & 1), writeC((word >> 24) & 1) {}

    std::uint8_t a, b, c, alu, shift, cond, address;
    bool useMbr, writeMbr, writeMar, read, write, writeC;
};

}  // namespace

MicroMachine::MicroMachine(Memory& memory) : memory(memory) {
    for (std::size_t i = 0; i < mic1Rom.size(); ++i)
        control.write(static_cast<std::uint8_t>(i), mic1Rom[i]);
}

MicroMachine::Snapshot MicroMachine::snapshot() const {
    return {cpuState, clock, pending, instructionCount, haltedFlag, memoryLatency, justCompleted};
}

void MicroMachine::restore(const Snapshot& state) {
    cpuState = state.cpu;
    clock = state.clock;
    pending = state.pending;
    instructionCount = state.instructions;
    haltedFlag = state.halted;
    memoryLatency = state.memoryLatency;
    justCompleted = state.justCompleted;
}

MicroCycleEvent MicroMachine::step() {
    if (haltedFlag)
        throw std::logic_error("simulação já encerrada");

    MicroCycleEvent event;
    event.mpc = cpuState.mpc;
    event.word = control.read(cpuState.mpc);
    event.memoryStall = pending.active && pending.repeatSeen && pending.remaining > 1;

    // A resposta da memória chega antes da ULA consumir os registradores.
    clock.tick([&] {
        justCompleted = false;
        advanceMemory(event);

        // Com latência 1, a repetição RD/WR ainda precisa concluir a
        // microinstrução nesta borda.
        if (event.memoryStall || (haltedFlag && pending.repeatSeen))
            return;

        executeMicro(event);
    });

    event.halted = haltedFlag;
    return event;
}

void MicroMachine::validateAddress(std::uint64_t address, bool write) const {
    if (address >= memory.sizeWords() || (!write && address >= 0xFFD))
        throw std::out_of_range("endereço fora da memória principal");
}

void MicroMachine::validateInstruction(Word word) {
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
}

void MicroMachine::advanceMemory(MicroCycleEvent& event) {
    if (!pending.active || --pending.remaining != 0)
        return;

    if (pending.read) {
        validateAddress(pending.address);
        cpuState.mbr.write(memory.read(pending.address));
    } else {
        validateAddress(pending.address, true);

        if (pending.address == 0xFFD) {
            haltedFlag = true;
        } else if (pending.address == 0xFFE) {
            event.characterOutput = true;
            event.output = pending.value & 0xFFu;
        } else if (pending.address == 0xFFF) {
            event.integerOutput = true;
            event.output = pending.value;
        } else {
            memory.write(pending.address, pending.value);
        }
    }

    pending.active = false;
    justCompleted = true;
}

void MicroMachine::validateMarSource(std::uint8_t mpc, Word raw) const {
    // Alguns caminhos precisam validar o endereço completo antes do corte em 12 bits.
    if (mpc == 0 && raw >= 0xFFD)
        throw std::out_of_range("PC fora da memória de programa");

    if ((mpc == 32 || mpc == 34 || mpc == 37 || mpc == 39) && std::uint64_t{cpuState.bank.read(RegisterBank::SP)} + (cpuState.bank.read(RegisterBank::IR) & 0xFFFu) >= 0xFFD)
        throw std::out_of_range("endereço relativo fora da RAM");

    if ((mpc == 48 || mpc == 53 || mpc == 55 || mpc == 56 || mpc == 61 || mpc == 62 || mpc == 67) && raw >= 0xFFD)
        throw std::out_of_range("endereço de pilha ou indireto fora da RAM");

    if (mpc == 58 && raw >= memory.sizeWords())
        throw std::out_of_range("endereço indireto fora da RAM");
}

void MicroMachine::executeMicro(MicroCycleEvent& event) {
    const std::uint64_t word = event.word;
    const std::uint8_t mpc = event.mpc;
    const MicroFields fields(word);

    const Word a = cpuState.bank.read(fields.a);
    const Word b = cpuState.bank.read(fields.b);

    const Word aluInput = AMUX::select(a, cpuState.mbr.read(), fields.useMbr);
    const auto result = ALU::execute(aluInput, b, fields.alu);
    const Word shifted = Shifter::execute(result.value, fields.shift);

    if (fields.read && fields.write)
        throw std::runtime_error("RD e WR simultâneos");

    const Word mar = fields.writeMar ? b & 0xFFFu : cpuState.mar.read();
    const Word mbr = fields.writeMbr ? shifted : cpuState.mbr.read();

    if (fields.writeMar)
        validateMarSource(mpc, b);

    // RD/WR repetido no microprograma sustenta a mesma transação pendente.
    if (fields.read || fields.write) {
        if (!pending.active) {
            if (!(justCompleted && pending.read == fields.read && pending.address == mar))
                pending = { true, fields.read, mar, mbr, memoryLatency, false };
        } else if (pending.read != fields.read) {
            throw std::runtime_error("transações de memória sobrepostas");
        } else {
            pending.repeatSeen = true;
        }
    }

    const std::uint8_t next = MMUX::select(mpc, fields.address, fields.cond, result.n, result.z);

    if (next == 0 && mpc != 0)
        ++instructionCount;

    // As escritas de registradores tornam-se visíveis ao fim deste ciclo.
    cpuState.a.write(a);
    cpuState.b.write(b);

    if (fields.writeMar)
        cpuState.mar.write(mar);

    if (fields.writeMbr)
        cpuState.mbr.write(shifted);

    if (fields.writeC) {
        if (fields.c == RegisterBank::IR)
            validateInstruction(shifted);

        cpuState.bank.write(fields.c, shifted);
    }

    cpuState.mir.write(word);
    cpuState.mpc = next;

    // flags capturadas
    cpuState.n = result.n;
    cpuState.z = result.z;
}
