#include "ProtocolCodec.hpp"
#include "ProtocolSession.hpp"
#include "../MicroMachine.hpp"
#include "../RAM.hpp"
#include "../StackInspector.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <poll.h>
#include <set>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace {

class Session {
public:
    int run() {
        const int flags = fcntl(STDIN_FILENO, F_GETFL);

        if (flags < 0 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
            throw std::runtime_error("não foi possível configurar stdin");

        while (!end) {
            if (!inputEof)
                receive(!running && frames.empty());

            while (!frames.empty() && !end) {
                tx::Frame frame = std::move(frames.front());
                frames.pop_front();

                try {
                    handle(frame);
                } catch (const std::exception& cause) {
                    std::cerr << "comando RX: " << cause.what() << '\n';
                    error(tx::Status::IoFailure, frame.type);
                    end = true;
                }
            }

            if (running && !end) {
                try {
                    runCycle();
                } catch (const std::exception& cause) {
                    std::cerr << "RUN: " << cause.what() << '\n';
                    error(tx::Status::IoFailure, tx::raw(tx::CommandType::Run));
                    end = true;
                }
            }

            if (inputEof && frames.empty() && !running)
                end = true;
        }

        return 0;
    }

private:
    struct Checkpoint {
        RAM ram;
        MicroMachine machine;
        MicroCycleEvent lastEvent;
        uint32_t memoryStalls;
        uint32_t outputCount;

        Checkpoint(const RAM& r, const MicroMachine& m, const MicroCycleEvent& e,
                   uint32_t stalls, uint32_t outputs)
            : ram(r), machine(m), lastEvent(e), memoryStalls(stalls), outputCount(outputs) {}
    };

    void rememberInstruction() {
        if (machine->cpu().mpc != 0)
            return;

        if (!history.empty() && history.back().machine.instructions() == machine->instructions())
            return;

        if (history.size() == 1024)
            history.pop_front();

        history.emplace_back(ram, *machine, lastEvent, memoryStalls, outputCount);
    }

    void emitOutput(const MicroCycleEvent& event) {
        if (event.integerOutput) {
            send(tx::ProgramOut{tx::OutputKind::SignedInteger, event.output});
            ++outputCount;
        }

        if (event.characterOutput) {
            send(tx::ProgramOut{tx::OutputKind::Character, event.output});
            ++outputCount;
        }
    }

    template <typename Message>
    void send(const Message& message) {
        const auto bytes = tx::toFrame(message);
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const ssize_t count = write(STDOUT_FILENO, bytes.data() + offset, bytes.size() - offset);
            if (count > 0) {
                offset += static_cast<std::size_t>(count);
            } else if (count < 0 && errno == EINTR) {
                continue;
            } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                pollfd output{STDOUT_FILENO, POLLOUT, 0};
                if (poll(&output, 1, -1) < 0 && errno != EINTR)
                    throw std::runtime_error("falha ao aguardar quadro TX");
            } else {
                throw std::runtime_error("falha ao escrever quadro TX");
            }
        }
    }

    void ack(uint8_t type, tx::Status status = tx::Status::Ok) { send(tx::Ack{type, status}); }
    void error(tx::Status status, uint8_t type) { send(tx::ErrorMsg{status, type}); }

    void receive(bool wait) {
        pollfd input{STDIN_FILENO, POLLIN, 0};
        const int ready = poll(&input, 1, wait ? -1 : 0);

        if (ready < 0) {
            if (errno == EINTR)
                return;

            throw std::runtime_error("falha ao aguardar RX");
        }

        if (ready == 0)
            return;

        if (input.revents & (POLLERR | POLLNVAL))
            throw std::runtime_error("falha no pipe RX");

        uint8_t bytes[4096];

        for (;;) {
            const ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));

            if (count > 0) {
                if (!decoder.feed(bytes, static_cast<std::size_t>(count),
                                   [this](tx::Frame frame) { frames.push_back(std::move(frame)); })) {
                    error(tx::Status::MalformedFrame, decoder.errorType());
                    end = true;
                    return;
                }
            } else if (count == 0) {
                if (decoder.incomplete()) {
                    error(tx::Status::MalformedFrame, decoder.currentType());
                    end = true;
                }

                inputEof = true;

                return;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else if (errno != EINTR) {
                throw std::runtime_error("falha ao ler RX");
            }
        }
    }

    uint32_t pc() const { return machine->cpu().bank.read(RegisterBank::PC); }

    bool breakpoint() const {
        return machine && machine->cpu().mpc == 0 && breakpoints.count(pc()) != 0;
    }

    tx::CycleState state(const MicroCycleEvent& event) const {
        tx::CycleState s;

        s.cycle = static_cast<uint32_t>(machine->cycles());
        s.mpc = event.mpc;
        s.microword = event.word;

        const auto& cpu = machine->cpu();

        s.flags = (cpu.n ? tx::CycleState::kFlagN : 0) |
                  (cpu.z ? tx::CycleState::kFlagZ : 0);

        s.mar = cpu.mar.read();
        s.mbr = cpu.mbr.read();

        if (event.memoryStall)
            s.stall = tx::StallReason::MemoryPending;

        for (std::size_t i = 0; i < s.regs.size(); ++i)
            s.regs[i] = cpu.bank.read(static_cast<uint8_t>(i));

        return s;
    }

    tx::Stats stats() const {
        tx::Stats s;

        if (machine) {
            s.cycles = static_cast<uint32_t>(machine->cycles());
            s.instructions = static_cast<uint32_t>(machine->instructions());
        }

        s.stallMemory = memoryStalls;

        return s;
    }

    void sendStackState(bool force = false) {
        if (!machine)
            return;

        StackSnapshot snapshot = StackInspector::capture(
            ram, machine->cpu().bank.read(RegisterBank::SP));

        if (!force && stackKnown && snapshot.sp == lastStack.sp &&
            snapshot.depth == lastStack.depth && snapshot.words == lastStack.words)
            return;

        tx::StackState message;
        message.cycle = static_cast<uint32_t>(machine->cycles());
        message.sp = snapshot.sp;
        message.depth = snapshot.depth;
        message.words = snapshot.words;
        send(message);

        lastStack = std::move(snapshot);
        stackKnown = true;
    }

    void step(bool report) {
        rememberInstruction();

        const auto event = machine->step();

        if (event.memoryStall)
            ++memoryStalls;

        lastEvent = event;

        if (report)
            send(state(event));

        sendStackState();

        emitOutput(event);

        if (event.halted)
            send(tx::Halted{static_cast<uint32_t>(machine->cycles()),
                            static_cast<uint32_t>(machine->instructions())});
    }

    void stopped(tx::StopReason reason) {
        running = false;
        pauseRequested = false;
    
        send(tx::RunStopped{reason, pc(), static_cast<uint32_t>(machine->cycles()),
                            static_cast<uint32_t>(machine->instructions())});
        send(stats());
    }

    void runCycle() {
        if (breakpoint()) {
            stopped(tx::StopReason::Breakpoint);
            return;
        }
    
        const auto before = machine->instructions();
        const bool complete = runConfig.reportMode == tx::ReportMode::Complete;
    
        rememberInstruction();
    
        const auto event = machine->step();
    
        if (event.memoryStall)
            ++memoryStalls;
    
        lastEvent = event;
        ++runCycles;
    
        const bool instructionDone = machine->instructions() != before;
        const bool compact = runConfig.reportMode == tx::ReportMode::Compact &&
                             (event.memoryStall || instructionDone || runCycles % runConfig.reportEvery == 0);
    
        if (complete || compact)
            send(state(event));

        sendStackState();
    
        emitOutput(event);
    
        if (event.halted) {
            send(tx::Halted{static_cast<uint32_t>(machine->cycles()),
                            static_cast<uint32_t>(machine->instructions())});
            stopped(tx::StopReason::Halt);
        } else if (pauseRequested) {
            stopped(tx::StopReason::Pause);
        } else if (runCycles >= runConfig.maxCycles) {
            stopped(tx::StopReason::CycleLimit);
        }
    }

    void installImage(const std::vector<uint32_t>& words) {
        ram.reset();

        for (std::size_t i = 0; i < words.size(); ++i)
            ram.write(i, words[i]);

        machine = std::make_unique<MicroMachine>(ram);
        machine->setMemoryLatency(config.memoryLatency);

        memoryStalls = 0;
        outputCount = 0;

        lastEvent = {};
        history.clear();
        stackKnown = false;
    }

    void handle(const tx::Frame& frame) {
        tx::Command command = tx::Pause{};
        const auto status = tx::decodeCommand(frame.type, frame.payload.data(), frame.payload.size(), command);

        if (status == tx::Status::Unsupported) {
            error(status, frame.type);
            return;
        }

        if (status != tx::Status::Ok) {
            ack(frame.type, status);
            return;
        }

        const auto type = static_cast<tx::CommandType>(frame.type);

        if (running && type != tx::CommandType::Pause) {
            ack(frame.type, tx::Status::BadState);
            return;
        }

        switch (type) {
            case tx::CommandType::LoadBegin: {
                const auto& c = std::get<tx::LoadBegin>(command);

                pending.clear();
                pendingCount = c.wordCount;
                loading = true;

                ack(frame.type);

                break;
            }
            case tx::CommandType::LoadData: {
                const auto& c = std::get<tx::LoadData>(command);

                if (!loading) {
                    ack(frame.type, tx::Status::BadState);
                    break;
                }

                if (c.offsetWords != pending.size()) {
                    ack(frame.type, tx::Status::BadValue);
                    break;
                }

                if (pending.size() + c.words.size() > pendingCount) {
                    ack(frame.type, tx::Status::AddressRange);
                    break;
                }

                pending.insert(pending.end(), c.words.begin(), c.words.end());

                ack(frame.type);

                break;
            }
            case tx::CommandType::LoadEnd:
                if (!loading) ack(frame.type, tx::Status::BadState);
                else if (pending.size() != pendingCount) ack(frame.type, tx::Status::LoadIncomplete);
                else {
                    image = std::move(pending);
                    loading = false;
                    installImage(image);
                    breakpoints.clear();

                    ack(frame.type);
                    sendStackState(true);
                }
                break;
            case tx::CommandType::StepCycle:
                if (!machine || machine->halted()) {
                    ack(frame.type, tx::Status::BadState);
                    break;
                }

                ack(frame.type);
                step(true);

                break;
            case tx::CommandType::StepInstr: {
                if (!machine || machine->halted()) {
                    ack(frame.type, tx::Status::BadState);
                    break;
                }

                ack(frame.type);

                const auto before = machine->instructions();

                while (!machine->halted() && machine->instructions() == before) {
                    if (breakpoint()) {
                        stopped(tx::StopReason::Breakpoint);
                        return;
                    }

                    step(false);
                }

                send(state(lastEvent));
                stopped(machine->halted() ? tx::StopReason::Halt : tx::StopReason::InstructionComplete);

                break;
            }
            case tx::CommandType::StepBack: {
                if (!machine || history.empty()) { ack(frame.type, tx::Status::BadState); break; }

                std::uint64_t targetInstruction = machine->instructions();
                if (machine->cpu().mpc == 0 && targetInstruction > 0)
                    --targetInstruction;

                while (!history.empty() && history.back().machine.instructions() > targetInstruction)
                    history.pop_back();

                if (history.empty() || history.back().machine.instructions() != targetInstruction) {
                    ack(frame.type, tx::Status::BadState);
                    break;
                }

                auto checkpoint = std::move(history.back());
                history.pop_back();
                ram = std::move(checkpoint.ram);
                machine = std::make_unique<MicroMachine>(checkpoint.machine);
                lastEvent = checkpoint.lastEvent;
                memoryStalls = checkpoint.memoryStalls;
                outputCount = checkpoint.outputCount;
                ack(frame.type);
                send(tx::Rewound{outputCount});
                send(state(lastEvent));
                sendStackState(true);
                send(stats());
                break;
            }
            case tx::CommandType::Run:
                if (!machine || machine->halted()) { ack(frame.type, tx::Status::BadState); break; }
                runConfig = std::get<tx::Run>(command);
                runCycles = 0;
                running = true;
                ack(frame.type);
                break;
            case tx::CommandType::Pause:
                if (!running) ack(frame.type, tx::Status::BadState);
                else { pauseRequested = true; ack(frame.type); }
                break;
            case tx::CommandType::Reset:
                if (!machine) {
                    ack(frame.type, tx::Status::BadState);
                } else {
                    installImage(image);
                    breakpoints.clear();
                    loading = false;
                    ack(frame.type);
                    sendStackState(true);
                }
                break;
            case tx::CommandType::SetBp:
                breakpoints.insert(std::get<tx::SetBp>(command).pcWord);
                ack(frame.type);
                break;
            case tx::CommandType::ClrBp:
                breakpoints.erase(std::get<tx::ClrBp>(command).pcWord);
                ack(frame.type);
                break;
            case tx::CommandType::ReadMem: {
                const auto& c = std::get<tx::ReadMem>(command);

                tx::MemData data;
                data.startWord = c.startWord;

                for (uint32_t i = 0; i < c.wordCount; ++i)
                    data.words.push_back(ram.read(c.startWord + i));

                ack(frame.type);
                send(data);

                break;
            }
            case tx::CommandType::Config:
                config = std::get<tx::Config>(command);

                if (machine)
                    machine->setMemoryLatency(config.memoryLatency);

                history.clear();

                ack(frame.type);

                break;
        }
    }

    RAM ram;
    std::unique_ptr<MicroMachine> machine;
    tx::FrameDecoder decoder;
    std::deque<tx::Frame> frames;
    std::vector<uint32_t> image;
    std::vector<uint32_t> pending;
    uint32_t pendingCount = 0;

    bool loading = false;
    bool running = false;
    bool pauseRequested = false;
    bool end = false;
    bool inputEof = false;

    tx::Run runConfig;
    uint32_t runCycles = 0;
    uint32_t memoryStalls = 0;
    tx::Config config{16, 16, 4, 2};
    std::set<uint32_t> breakpoints;
    MicroCycleEvent lastEvent;
    std::deque<Checkpoint> history;
    uint32_t outputCount = 0;
    StackSnapshot lastStack;
    bool stackKnown = false;
};

}  // namespace

int runProtocolSession() {
    try {
        return Session{}.run();
    } catch (const std::exception& error) {
        std::cerr << "protocolo: " << error.what() << '\n';
        return 1;
    }
}
