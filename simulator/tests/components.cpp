#include "ALU.hpp"
#include "AMUX.hpp"
#include "Clock.hpp"
#include "ControlStore.hpp"
#include "MMUX.hpp"
#include "io/BinaryImage.hpp"
#include "RAM.hpp"
#include "Shifter.hpp"
#include "StackInspector.hpp"
#include "registers/CPURegisters.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Exception, typename Action>

void checkThrows(Action action, const char* message) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }

    throw std::runtime_error(message);
}

void testDatapath() {
    const auto zero = ALU::execute(0xFFFFFFFFu, 1, 0b00);

    check(zero.value == 0 && zero.z && !zero.n, "ALU: soma com overflow e flag Z");

    const auto negative = ALU::execute(0x80000000u, 0xFFFFFFFFu, 0b01);

    check(negative.value == 0x80000000u && negative.n && !negative.z, "ALU: AND e flag N");
    check(ALU::execute(42, 0, 0b10).value == 42, "ALU: passagem de A");
    check(ALU::execute(0, 0, 0b11).value == 0xFFFFFFFFu, "ALU: inversao de A");

    checkThrows<std::invalid_argument>([] { ALU::execute(0, 0, 4); }, "ALU: operacao invalida");

    check(Shifter::execute(0x80000001u, 0b00) == 0x80000001u, "shifter: sem deslocamento");
    check(Shifter::execute(0x80000001u, 0b01) == 0x40000000u, "shifter: deslocamento logico a direita");
    check(Shifter::execute(0x80000001u, 0b10) == 2u, "shifter: deslocamento logico a esquerda");
    checkThrows<std::invalid_argument>([] { Shifter::execute(0, 3); }, "shifter: controle invalido");

    check(AMUX::select(10, 20, 0) == 10 && AMUX::select(10, 20, 1) == 20, "AMUX: selecao de entrada");
    checkThrows<std::invalid_argument>([] { AMUX::select(0, 0, 2); }, "AMUX: controle invalido");
    check(MMUX::select(7, 42, 0b00, true, true) == 8, "MMUX: proximo endereco");
    check(MMUX::select(7, 42, 0b01, true, false) == 42, "MMUX: desvio por N");
    check(MMUX::select(7, 42, 0b10, false, true) == 42, "MMUX: desvio por Z");
    check(MMUX::select(7, 42, 0b11, false, false) == 42, "MMUX: desvio direto");
    check(MMUX::select(255, 42, 0b00, false, false) == 0, "MMUX: retorno a zero");
}

void testState() {
    CPURegisters cpu;

    check(cpu.bank.read(RegisterBank::ZERO) == 0 &&
          cpu.bank.read(RegisterBank::ONE) == 1 &&
          cpu.bank.read(RegisterBank::SP) == 0xFFCu, "registradores: estado inicial");

    cpu.bank.write(RegisterBank::ZERO, 5);
    cpu.bank.write(RegisterBank::ONE, 5);

    check(cpu.bank.read(RegisterBank::ZERO) == 0 && cpu.bank.read(RegisterBank::ONE) == 1, "registradores: constantes imutaveis");

    cpu.bank.write(RegisterBank::PC, 9, false);

    check(cpu.bank.read(RegisterBank::PC) == 0, "registradores: escrita desabilitada");

    cpu.bank.write(RegisterBank::PC, 9);
    cpu.a.write(4);
    cpu.mpc = 8;
    cpu.reset();

    check(cpu.bank.read(RegisterBank::PC) == 0 && cpu.a.read() == 0 && cpu.mpc == 0,
          "registradores: reset");

    Clock clock;
    int edges = 0;
    clock.tick([&] { ++edges; });

    check(edges == 1 && clock.cycles() == 1, "clock: borda e contagem");

    clock.reset();

    check(clock.cycles() == 0, "clock: reset");

    ControlStore store;
    store.write(255, ControlStore::wordMask);

    check(store.read(255) == ControlStore::wordMask, "memoria de controle: 36 bits");

    checkThrows<std::out_of_range>([&] { store.write(0, ControlStore::wordMask + 1); }, "memoria de controle: limite de 36 bits");

    store.reset();
    check(store.read(255) == 0, "memoria de controle: reset");

    RAM ram;
    ram.write(RAM::size - 1, 0x12345678u);

    check(ram.read(RAM::size - 1) == 0x12345678u, "RAM: ultima palavra");

    checkThrows<std::out_of_range>([&] { ram.read(RAM::size); }, "RAM: leitura fora do limite");
    checkThrows<std::out_of_range>([&] { ram.write(RAM::size, 0); }, "RAM: escrita fora do limite");

    ram.reset();

    check(ram.read(RAM::size - 1) == 0, "RAM: reset");
}

void testStackInspector() {
    RAM ram;
    auto snapshot = StackInspector::capture(ram, 0xFFC);
    check(snapshot.depth == 0 && snapshot.words.empty(), "pilha: estado inicial vazio");

    ram.write(0xFFB, 42);
    ram.write(0xFFA, 17);
    snapshot = StackInspector::capture(ram, 0xFFA);
    check(snapshot.sp == 0xFFA && snapshot.depth == 2 &&
          snapshot.words == std::vector<Word>({17, 42}), "pilha: topo e ordem das palavras");

    snapshot = StackInspector::capture(ram, 0xFFB);
    check(snapshot.depth == 1 && snapshot.words == std::vector<Word>({42}),
          "pilha: palavra removida não aparece");

    snapshot = StackInspector::capture(ram, 0);
    check(snapshot.depth == 0xFFC && snapshot.words.size() == StackInspector::maxVisibleWords,
          "pilha: profundidade total e janela limitada");

    snapshot = StackInspector::capture(ram, 0xFFD);
    check(snapshot.depth == 0 && snapshot.words.empty(), "pilha: SP fora da região");
}

void writeBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary);

    check(static_cast<bool>(file), "teste: criacao do arquivo binario");

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    check(static_cast<bool>(file), "teste: escrita do arquivo binario");
}

void testLoader(const std::filesystem::path& directory) {
    const auto path = directory / "components_program.bin";

    struct Cleanup {
        std::filesystem::path path;

        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } cleanup{path};

    RAM ram;
    ram.write(10, 99);

    writeBytes(path, {0x78, 0x56, 0x34, 0x12, 0xFF, 0x00, 0x00, 0x80});

    check(loadBinaryImage(path.string(), ram) == 2, "loader: contagem de palavras");
    check(ram.read(0) == 0x12345678u && ram.read(1) == 0x800000FFu, "loader: palavras little-endian");
    check(ram.read(10) == 0, "loader: limpa RAM antes de carregar");

    writeBytes(path, {0x12, 0x34, 0x56});
    checkThrows<std::runtime_error>([&] { loadBinaryImage(path.string(), ram); }, "loader: rejeita palavra incompleta");

    writeBytes(path, std::vector<unsigned char>(RAM::sizeBytes + 4, 0));
    checkThrows<std::runtime_error>([&] { loadBinaryImage(path.string(), ram); }, "loader: rejeita programa maior que a RAM");
    checkThrows<std::runtime_error>([&] { loadBinaryImage((path.string() + ".missing"), ram); }, "loader: rejeita arquivo inexistente");
}

int main(int argc, char* argv[]) {
    try {
        check(argc == 2, "uso: mic3_tests diretorio_temporario");

        testDatapath();
        testState();
        testStackInspector();
        testLoader(argv[1]);

        std::cout << "Todos os testes de componentes passaram\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
