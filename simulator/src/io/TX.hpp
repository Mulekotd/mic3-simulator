#pragma once

#include "Protocol.hpp"

#include <array>
#include <stdexcept>
#include <vector>

namespace tx {

// mensagens TX (sim -> GUI)

struct Ack {
    static constexpr MessageType kType = MessageType::Ack;
    static constexpr std::size_t kLen = 2;

    uint8_t commandType = 0;
    Status status = Status::Ok;

    void encode(ByteWriter& w) const {
        w.u8(commandType);
        w.u8(raw(status));
    }
};

struct ErrorMsg {
    static constexpr MessageType kType = MessageType::Error;
    static constexpr std::size_t kLen = 2;

    Status status = Status::Ok;
    uint8_t offendingType = 0;

    void encode(ByteWriter& w) const {
        w.u8(raw(status));
        w.u8(offendingType);
    }
};

struct CycleState {
    static constexpr MessageType kType = MessageType::CycleState;
    static constexpr std::size_t kLen = 174;

    static constexpr uint8_t  kFlagN = 1u << 0;
    static constexpr uint8_t  kFlagZ = 1u << 1;
    static constexpr uint8_t  kPipeRead = 1u << 0;
    static constexpr uint8_t  kPipeExecute = 1u << 1;
    static constexpr uint8_t  kPipeWriteback = 1u << 2;
    static constexpr uint8_t  kDCacheWrite = 1u << 0;
    static constexpr uint16_t kNoUop = 0xFFFF;

    uint32_t cycle = 0;
    uint16_t mpc = 0;
    uint64_t microword = 0;  // somente 36 bits usados
    uint8_t flags = 0;
    uint32_t mar = 0;
    uint32_t mbr = 0;

    uint8_t pipeValid = 0;
    std::array<uint16_t, 3> pipeUop{kNoUop, kNoUop, kNoUop};
    StallReason stall = StallReason::None;

    std::array<uint32_t, kRegCount> regs{};

    CacheResult icacheResult = CacheResult::None;
    uint16_t icacheIndex = 0;
    uint32_t icacheTag = 0;

    CacheResult dcacheResult = CacheResult::None;
    uint16_t dcacheIndex = 0;
    uint32_t dcacheTag = 0;
    uint8_t dcacheFlags = 0;

    void encode(ByteWriter& w) const {
        w.u32(cycle);
        w.u16(mpc);
        w.u64(microword & 0xFFFFFFFFFull);
        w.u8(flags);
        w.u32(mar);
        w.u32(mbr);
        w.u8(pipeValid);

        for (uint16_t u : pipeUop)
            w.u16(u);

        w.u8(static_cast<uint8_t>(stall));

        for (uint32_t r : regs)
            w.u32(r);

        w.u8(static_cast<uint8_t>(icacheResult));
        w.u16(icacheIndex);
        w.u32(icacheTag);

        w.u8(static_cast<uint8_t>(dcacheResult));
        w.u16(dcacheIndex);
        w.u32(dcacheTag);
        w.u8(dcacheFlags);
    }
};

struct ProgramOut {
    static constexpr MessageType kType = MessageType::ProgramOut;
    static constexpr std::size_t kLen = 5;

    OutputKind kind = OutputKind::SignedInteger;
    uint32_t value = 0;  // kind=Character usa apenas o byte baixo

    void encode(ByteWriter& w) const {
        w.u8(static_cast<uint8_t>(kind));
        w.u32(value);
    }
};

struct MemData {
    static constexpr MessageType kType = MessageType::MemData;

    // LEN = 6 + 4 * words.size() (1 <= n <= 64)
    uint32_t startWord = 0;
    std::vector<uint32_t> words;

    void encode(ByteWriter& w) const {
        if (words.empty() || words.size() > kMaxWordsPerFrame)
            throw std::invalid_argument("MEM_DATA exige de 1 a 64 palavras");

        w.u32(startWord);
        w.u16(static_cast<uint16_t>(words.size()));

        for (uint32_t x : words)
            w.u32(x);
    }
};

struct Halted {
    static constexpr MessageType kType = MessageType::Halted;
    static constexpr std::size_t kLen = 8;

    uint32_t cycles = 0;
    uint32_t instructions = 0;

    void encode(ByteWriter& w) const {
        w.u32(cycles);
        w.u32(instructions);
    }
};

struct Stats {
    static constexpr MessageType kType = MessageType::Stats;
    static constexpr std::size_t kLen = 52;

    uint32_t cycles = 0;
    uint32_t instructions = 0;

    uint32_t icacheAccesses = 0;
    uint32_t icacheHits = 0;
    uint32_t icacheMisses = 0;

    uint32_t dcacheAccesses = 0;
    uint32_t dcacheHits = 0;
    uint32_t dcacheMisses = 0;

    uint32_t stallRaw = 0;
    uint32_t stallBranch = 0;
    uint32_t stallIcache = 0;
    uint32_t stallDcache = 0;
    uint32_t stallMemory = 0;

    void encode(ByteWriter& w) const {
        w.u32(cycles);
        w.u32(instructions);
        w.u32(icacheAccesses);
        w.u32(icacheHits);
        w.u32(icacheMisses);
        w.u32(dcacheAccesses);
        w.u32(dcacheHits);
        w.u32(dcacheMisses);
        w.u32(stallRaw);
        w.u32(stallBranch);
        w.u32(stallIcache);
        w.u32(stallDcache);
        w.u32(stallMemory);
    }
};

struct RunStopped {
    static constexpr MessageType kType = MessageType::RunStopped;
    static constexpr std::size_t kLen = 13;

    StopReason reason = StopReason::CycleLimit;
    uint32_t pcWord = 0;
    uint32_t cycles = 0;
    uint32_t instructions = 0;

    void encode(ByteWriter& w) const {
        w.u8(static_cast<uint8_t>(reason));
        w.u32(pcWord);
        w.u32(cycles);
        w.u32(instructions);
    }
};

struct Rewound {
    static constexpr MessageType kType = MessageType::Rewound;
    static constexpr std::size_t kLen = 4;
    uint32_t outputCount = 0;
    void encode(ByteWriter& w) const { w.u32(outputCount); }
};

// Monta um quadro completo: TYPE | LEN (u16 LE) | PAYLOAD.
// Devolve vetor vazio se o payload exceder kMaxPayload.
inline std::vector<uint8_t> makeFrame(uint8_t type, const std::vector<uint8_t>& payload) {
    if (payload.size() > kMaxPayload)
        return {};

    std::vector<uint8_t> frame;
    frame.reserve(kHeaderSize + payload.size());

    ByteWriter w(frame);
    w.u8(type);
    w.u16(static_cast<uint16_t>(payload.size()));

    frame.insert(frame.end(), payload.begin(), payload.end());

    return frame;
}

template <typename Message>

std::vector<uint8_t> toFrame(const Message& m) {
    std::vector<uint8_t> payload;
    ByteWriter w(payload);
    m.encode(w);
    return makeFrame(raw(Message::kType), payload);
}

}  // namespace tx
