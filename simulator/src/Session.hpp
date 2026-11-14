#pragma once

#include "MicroMachine.hpp"
#include "RAM.hpp"
#include "io/BinaryImage.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class Session {
public:
    void load(const std::string& path) {
        RAM loaded;
        loadBinaryImage(path, loaded);

        ram = loaded;
        machine = std::make_unique<MicroMachine>(ram);

        program = path;

        history.clear();
        output.clear();

        running = false;
    }

    void reset() {
        if (program.empty())
            throw std::runtime_error("nenhum programa carregado");

        load(program);
    }

    bool step() {
        if (!machine || machine->halted())
            return false;

        history.push_back({ ram, machine->snapshot(), output.size() });

        if (history.size() > 1024)
            history.pop_front();

        try {
            const auto event = machine->step();

            if (event.integerOutput) {
                const std::int64_t signedValue = event.output <= 0x7FFFFFFFu
                    ? event.output : std::int64_t{event.output} - 0x100000000LL;

                output.push_back("PROGRAM_OUT=" + std::to_string(signedValue));
            }

            if (event.characterOutput)
                output.push_back(std::string(1, static_cast<char>(event.output)));

            if (machine->halted())
                running = false;
        } catch (...) {
            restoreLast();
            throw;
        }

        return true;
    }

    bool back() {
        running = false;
        return restoreLast();
    }

    void run() {
        if (machine && !machine->halted())
            running = true;
    }

    void pause() { running = false; }
    bool isRunning() const { return running; }
    bool isLoaded() const { return static_cast<bool>(machine); }
    bool isHalted() const { return machine && machine->halted(); }

    std::uint64_t cycles() const { return machine ? machine->cycles() : 0; }
    std::uint64_t instructions() const { return machine ? machine->instructions() : 0; }

    const std::string& path() const { return program; }
    const std::vector<std::string>& outputs() const { return output; }
    const CPURegisters* cpu() const { return machine ? &machine->cpu() : nullptr; }

private:
    struct History {
        RAM ram;
        MicroMachine::Snapshot machine;
        std::size_t outputCount;
    };

    bool restoreLast() {
        if (history.empty())
            return false;

        ram = history.back().ram;
        machine->restore(history.back().machine);
        output.resize(history.back().outputCount);
        history.pop_back();

        return true;
    }

    RAM ram;
    std::unique_ptr<MicroMachine> machine;
    std::string program;
    std::deque<History> history;
    std::vector<std::string> output;

    bool running = false;
};
