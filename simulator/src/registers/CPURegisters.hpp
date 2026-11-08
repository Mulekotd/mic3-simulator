#pragma once

#include "RegisterBank.hpp"
#include "MIR.hpp"
#include "../Latch.hpp"

#include <cstdint>

struct CPURegisters {
    RegisterBank bank; // PC, AC, SP, IR e TIR
    Latch a;
    Latch b;
    Latch mar;
    Latch mbr;
    MIR mir;
    std::uint8_t mpc = 0; // endereço da memória de controle

    bool n = false; // sinal de negativo
    bool z = false; // sinal de zero

    void reset() {
        bank.reset();

        a.reset();
        b.reset();
        mar.reset();
        mbr.reset();

        mir.reset();

        mpc = 0;

        n = false;
        z = false;
    }
};
