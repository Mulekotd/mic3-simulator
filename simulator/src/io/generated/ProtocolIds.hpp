// Gerado por tools/generate_protocol_cpp.py; não editar.
#pragma once

#include <cstddef>
#include <cstdint>

namespace tx {

constexpr std::size_t kMaxPayload = 512;
constexpr uint32_t kMaxProgramWords = 4096;
constexpr uint32_t kMaxWordsPerFrame = 64;
constexpr uint32_t kMaxBreakpointPc = 4095;
constexpr uint32_t kRamEnd = 4093;
constexpr std::size_t kRegCount = 32;

enum class Status : uint8_t {
    Ok = 0,
    BadLength = 1,
    BadState = 2,
    AddressRange = 3,
    BadValue = 4,
    Unsupported = 5,
    LoadIncomplete = 6,
    IoFailure = 7,
    MalformedFrame = 8,
};

enum class ReportMode : uint8_t {
    Complete = 0,
    Compact = 1,
    FinalOnly = 2,
};

enum class OutputKind : uint8_t {
    SignedInteger = 0,
    Character = 1,
};

enum class StopReason : uint8_t {
    CycleLimit = 0,
    Pause = 1,
    Breakpoint = 2,
    Halt = 3,
    InstructionComplete = 4,
};

enum class CacheResult : uint8_t {
    None = 0,
    Hit = 1,
    Miss = 2,
    Bypass = 3,
};

enum class StallReason : uint8_t {
    None = 0,
    Raw = 1,
    Branch = 2,
    ICacheMiss = 3,
    DCacheMiss = 4,
    MemoryPending = 5,
};

enum class CommandType : uint8_t {
    LoadBegin = 0x01,
    LoadData = 0x02,
    LoadEnd = 0x03,
    StepCycle = 0x10,
    StepInstr = 0x11,
    Run = 0x12,
    Pause = 0x13,
    Reset = 0x14,
    SetBp = 0x15,
    ClrBp = 0x16,
    StepBack = 0x17,
    ReadMem = 0x20,
    Config = 0x21,
};

enum class MessageType : uint8_t {
    Ack = 0x81,
    CycleState = 0x90,
    ProgramOut = 0x91,
    MemData = 0x92,
    Halted = 0x93,
    Stats = 0x94,
    RunStopped = 0x95,
    Rewound = 0x96,
    Error = 0x9F,
};

constexpr uint8_t raw(CommandType t) { return static_cast<uint8_t>(t); }
constexpr uint8_t raw(MessageType t) { return static_cast<uint8_t>(t); }
constexpr uint8_t raw(Status s) { return static_cast<uint8_t>(s); }

}  // namespace tx
