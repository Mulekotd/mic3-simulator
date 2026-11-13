#include "io/ProtocolCodec.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <variant>
#include <vector>

void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

int main() {
    try {
        const auto ack = tx::toFrame(tx::Ack{tx::raw(tx::CommandType::StepCycle), tx::Status::Ok});

        check(ack == std::vector<uint8_t>({ 0x81, 0x02, 0x00, 0x10, 0x00 }), "ACK divergente do protocolo");

        tx::Command command = tx::Pause{};
        const uint8_t loadBegin[] = { 0, 0, 0, 0, 1, 0, 0, 0 };

        check(tx::decodeCommand(0x01, loadBegin, sizeof(loadBegin), command) == tx::Status::Ok, "LOAD_BEGIN valido rejeitado");
        check(std::get<tx::LoadBegin>(command).wordCount == 1, "LOAD_BEGIN decodificado incorretamente");
        check(tx::decodeCommand(0x01, loadBegin, 7, command) == tx::Status::BadLength, "LOAD_BEGIN truncado aceito");
        check(tx::decodeCommand(0x77, nullptr, 0, command) == tx::Status::Unsupported, "TYPE desconhecido aceito");
        check(tx::decodeCommand(0x10, nullptr, 0, command) == tx::Status::Ok, "comando sem payload rejeitado");

        const auto cycle = tx::toFrame(tx::CycleState{});
    
        check(cycle.size() == tx::kHeaderSize + tx::CycleState::kLen && cycle[1] == 174 && cycle[2] == 0, "CYCLE_STATE com tamanho incorreto");

        const auto mem = tx::toFrame(tx::MemData{0x1234, {0x12345678}});
    
        check(mem == std::vector<uint8_t>({0x92, 0x0A, 0x00, 0x34, 0x12, 0x00, 0x00, 0x01, 0x00, 0x78, 0x56, 0x34, 0x12}), "MEM_DATA com layout incorreto");

        bool rejected = false;

        try {
            tx::toFrame(tx::MemData{0, {}});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        check(rejected, "MEM_DATA vazio aceito");

        rejected = false;

        try {
            tx::toFrame(tx::MemData{0, std::vector<uint32_t>(65)});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }

        check(rejected, "MEM_DATA com 65 palavras aceito");

        const auto stack = tx::toFrame(tx::StackState{7, 0xFFB, 1, {42}});
        check(stack == std::vector<uint8_t>({0x97, 0x0F, 0x00,
                                             7, 0, 0, 0,
                                             0xFB, 0x0F, 0, 0,
                                             1, 0, 1,
                                             42, 0, 0, 0}),
              "STACK_STATE com layout incorreto");

        rejected = false;
        try {
            tx::toFrame(tx::StackState{0, 0, 33, std::vector<uint32_t>(33)});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "STACK_STATE com mais de 32 palavras aceito");

        std::cout << "Protocolo TX/RX passou\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
