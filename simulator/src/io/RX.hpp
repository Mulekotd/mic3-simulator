#pragma once

#include "Protocol.hpp"

#include <utility>
#include <variant>
#include <vector>

namespace tx {

// comandos RX (GUI -> sim)

struct LoadBegin {
    uint32_t startWord = 0;
    uint32_t wordCount = 0;
};

struct LoadData {
    uint32_t offsetWords = 0;
    std::vector<uint32_t> words;
};

struct LoadEnd {};

struct StepCycle {};

struct StepInstr {};
struct StepBack {};

struct Run {
    uint32_t maxCycles = 0;
    ReportMode reportMode = ReportMode::Complete;
    uint16_t reportEvery = 1;
};

struct Pause {};

struct Reset {};

struct SetBp {
    uint32_t pcWord = 0;
};

struct ClrBp {
    uint32_t pcWord = 0;
};

struct ReadMem {
    uint32_t startWord = 0;
    uint16_t wordCount = 0;
};

struct Config {
    uint16_t iLines = 0;
    uint16_t dLines = 0;
    uint8_t lineWords = 0;
    uint16_t memoryLatency = 0;
};

using Command = std::variant<LoadBegin, LoadData, LoadEnd, StepCycle, StepInstr, StepBack, Run,
                             Pause, Reset, SetBp, ClrBp, ReadMem, Config>;

inline bool isKnownCommand(uint8_t type) {
    switch (type) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x20:
        case 0x21:
            return true;
        default:
            return false;
    }
}

namespace detail {
constexpr bool isPow2In(uint32_t v, uint32_t lo, uint32_t hi) {
    return v >= lo && v <= hi && (v & (v - 1)) == 0;
}
}  // namespace detail

// Decodifica e valida a parte "estática" de um comando (tamanho e faixas do
// protocol.yaml). Verificações que dependem de estado (programa carregado,
// simulador parado, offsets sequenciais, etc.) ficam com o chamador.
//
// Retorno:
//   Ok            -> `out` preenchido
//   Unsupported   -> TYPE desconhecido (responder ERROR(unsupported, type))
//   BadLength     -> comando conhecido com tamanho incorreto (ACK com bad_length)
//   BadValue / AddressRange -> campo fora da faixa (ACK com o status)
inline Status decodeCommand(uint8_t type, const uint8_t* data, std::size_t len, Command& out) {
    ByteReader r(data, len);

    switch (type) {
        case 0x01: {  // LOAD_BEGIN
            if (len != 8)
                return Status::BadLength;

            LoadBegin c;
            c.startWord = r.u32();
            c.wordCount = r.u32();

            if (c.startWord != 0 || c.wordCount > kMaxProgramWords)
                return Status::BadValue;

            out = c;

            return Status::Ok;
        }
        case 0x02: {  // LOAD_DATA
            if (len < 8 || (len - 4) % 4 != 0)
                return Status::BadLength;

            const std::size_t n = (len - 4) / 4;

            if (n < 1 || n > kMaxWordsPerFrame)
                return Status::BadLength;

            LoadData c;
            c.offsetWords = r.u32();
            c.words.reserve(n);

            for (std::size_t i = 0; i < n; ++i)
                c.words.push_back(r.u32());

            out = std::move(c);

            return Status::Ok;
        }
        case 0x03:
            if (len != 0)
                return Status::BadLength;

            out = LoadEnd{};

            return Status::Ok;
        case 0x10:
            if (len != 0)
                return Status::BadLength;

            out = StepCycle{};

            return Status::Ok;
        case 0x11:
            if (len != 0)
                return Status::BadLength;

            out = StepInstr{};

            return Status::Ok;
        case 0x12: {  // RUN
            if (len != 7)
                return Status::BadLength;

            Run c;
            c.maxCycles = r.u32();
            const uint8_t mode = r.u8();
            c.reportEvery = r.u16();

            if (c.maxCycles < 1 || c.reportEvery < 1 ||
                mode > static_cast<uint8_t>(ReportMode::FinalOnly))
                return Status::BadValue;

            c.reportMode = static_cast<ReportMode>(mode);

            out = c;

            return Status::Ok;
        }
        case 0x13:
            if (len != 0)
                return Status::BadLength;

            out = Pause{};

            return Status::Ok;
        case 0x14:
            if (len != 0)
                return Status::BadLength;

            out = Reset{};

            return Status::Ok;
        case 0x17:
            if (len != 0)
                return Status::BadLength;
            out = StepBack{};
            return Status::Ok;
        case 0x15:    // SET_BP
        case 0x16: {  // CLR_BP
            if (len != 4)
                return Status::BadLength;

            const uint32_t pc = r.u32();

            if (pc > kMaxBreakpointPc)
                return Status::AddressRange;

            if (type == 0x15)
                out = SetBp{pc};
            else
                out = ClrBp{pc};

            return Status::Ok;
        }
        case 0x20: {  // READ_MEM
            if (len != 6)
                return Status::BadLength;

            ReadMem c;
            c.startWord = r.u32();
            c.wordCount = r.u16();

            if (c.wordCount < 1 || c.wordCount > kMaxWordsPerFrame)
                return Status::BadValue;

            if (static_cast<uint64_t>(c.startWord) + c.wordCount > kRamEnd)
                return Status::AddressRange;

            out = c;

            return Status::Ok;
        }
        case 0x21: {  // CONFIG
            if (len != 7)
                return Status::BadLength;

            Config c;
            c.iLines = r.u16();
            c.dLines = r.u16();
            c.lineWords = r.u8();
            c.memoryLatency = r.u16();

            const bool lwOk = c.lineWords == 1 || c.lineWords == 2 ||
                               c.lineWords == 4 || c.lineWords == 8;

            if (!detail::isPow2In(c.iLines, 1, 256) || !detail::isPow2In(c.dLines, 1, 256) || !lwOk || c.memoryLatency < 1)
                return Status::BadValue;

            out = c;

            return Status::Ok;
        }
        default:
            return Status::Unsupported;
    }
}

}  // namespace tx
