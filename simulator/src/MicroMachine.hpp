#pragma once

#include "Clock.hpp"
#include "ControlStore.hpp"
#include "Memory.hpp"
#include "registers/CPURegisters.hpp"

#include <cstdint>

// Retrato da microinstrução executada e dos efeitos observáveis no ciclo.
struct MicroCycleEvent {
    std::uint8_t mpc = 0;
    std::uint64_t word = 0;
    bool integerOutput = false;
    bool characterOutput = false;
    Word output = 0;
    bool halted = false;
    bool memoryStall = false;
};

class MicroMachine {
private:
    // Transação pendente da RAM; a latência padrão é de dois ciclos.
    struct Pending {
        bool active = false;
        bool read = false;
        Word address = 0;
        Word value = 0;
        unsigned remaining = 0;
        bool repeatSeen = false;
    };

public:
    struct Snapshot {
        CPURegisters cpu;
        Clock clock;
        Pending pending;
        std::uint64_t instructions;
        bool halted;
        unsigned memoryLatency;
        bool justCompleted;
    };

    explicit MicroMachine(Memory& memory);

    MicroCycleEvent step();
    Snapshot snapshot() const;
    void restore(const Snapshot& state);
    const CPURegisters& cpu() const { return cpuState; }
    std::uint64_t cycles() const { return clock.cycles(); }
    std::uint64_t instructions() const { return instructionCount; }
    bool halted() const { return haltedFlag; }
    void setMemoryLatency(unsigned cycles) { memoryLatency = cycles; }

private:

    Memory& memory;
    CPURegisters cpuState;
    ControlStore control;
    Clock clock;
    Pending pending;
    std::uint64_t instructionCount = 0;
    bool haltedFlag = false;
    unsigned memoryLatency = 2;
    bool justCompleted = false;

    void validateAddress(std::uint64_t address, bool write = false) const;
    static void validateInstruction(Word word);
    void advanceMemory(MicroCycleEvent& event);
    void validateMarSource(std::uint8_t mpc, Word raw) const;
    void executeMicro(MicroCycleEvent& event);
};
