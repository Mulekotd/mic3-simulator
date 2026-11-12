#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t ramWords = 4096;

// Mantém a posição original para apontar erros com linha e coluna.
struct SourceLine {
    std::size_t number;
    std::string text;
};

struct Statement {
    std::uint32_t address;
    std::size_t line;
    std::size_t column;
    std::string operation;
    std::string operand;
    std::size_t operandColumn;
    std::string source;
};

struct Program {
    // A listagem acompanha cada palavra, inclusive os espaços criados por .org.
    std::vector<std::uint32_t> words;
    std::vector<std::string> sources;
    std::vector<std::size_t> lines;
};

class Assembler {
public:
    explicit Assembler(std::filesystem::path input) : input(std::move(input)) {}

    Program assemble() {
        readSource();
        firstPass(); // Coleta rótulos
        return secondPass(); // Resolve operandos e codifica
    }

private:
    std::filesystem::path input;
    std::vector<SourceLine> lines;
    std::vector<Statement> statements;
    std::map<std::string, std::uint32_t> symbols;
    std::uint32_t imageWords = 0;

    [[noreturn]] void error(std::size_t line, std::size_t column, const std::string& message) const {
        throw std::runtime_error(input.string() + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + message);
    }

    static bool identifierStart(char ch) {
        const auto c = static_cast<unsigned char>(ch);
        return std::isalpha(c) || ch == '_';
    }

    static bool identifierPart(char ch) {
        const auto c = static_cast<unsigned char>(ch);
        return std::isalnum(c) || ch == '_';
    }

    static bool identifier(std::string_view value) {
        if (value.empty() || !identifierStart(value.front()))
            return false;

        return std::all_of(value.begin() + 1, value.end(), identifierPart);
    }

    static std::string upper(std::string value) {
        for (char& ch : value)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        return value;
    }

    void readSource() {
        std::ifstream file(input);

        if (!file)
            throw std::runtime_error("não foi possível abrir: " + input.string());

        std::string text;

        for (std::size_t number = 1; std::getline(file, text); ++number) {
            if (!text.empty() && text.back() == '\r')
                text.pop_back();

            lines.push_back({number, text});
        }

        if (file.bad())
            throw std::runtime_error("erro ao ler: " + input.string());
    }

    static void skipSpace(const std::string& text, std::size_t& pos) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
    }

    std::int64_t number(std::string_view token, std::size_t line, std::size_t column) const {
        // Aceita decimal e hexadecimal, com sinal opcional.
        bool negative = false;

        if (!token.empty() && (token.front() == '-' || token.front() == '+')) {
            negative = token.front() == '-';
            token.remove_prefix(1);
        }

        int base = 10;

        if (token.size() >= 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            token.remove_prefix(2);
            base = 16;
        }

        if (token.empty())
            error(line, column, "número inválido");

        std::uint64_t magnitude = 0;

        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), magnitude, base);

        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() || magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            error(line, column, "número inválido ou fora da faixa");

        const auto value = static_cast<std::int64_t>(magnitude);

        return negative ? -value : value;
    }

    std::int64_t operandValue(const Statement& statement) const {
        // Rótulos são resolvidos somente após a primeira passagem.
        if (identifier(statement.operand)) {
            const auto found = symbols.find(statement.operand);

            if (found == symbols.end())
                error(statement.line, statement.operandColumn, "rótulo indefinido: " + statement.operand);

            return found->second;
        }

        return number(statement.operand, statement.line, statement.operandColumn);
    }

    // Separa operações com e sem operando para validar a sintaxe na leitura.

    static bool needsOperand(const std::string& op) {
        static const std::array<std::string_view, 19> names = {
            "LODD", "STOD", "ADDD", "SUBD", "JPOS", "JZER", "JUMP", "LOCO",
            "LODL", "STOL", "ADDL", "SUBL", "JNEG", "JNZE", "CALL", "INSP",
            "DESP", ".WORD", ".ORG"
        };

        return std::find(names.begin(), names.end(), op) != names.end();
    }

    static bool noOperand(const std::string& op) {
        static const std::array<std::string_view, 9> names = {
            "PSHI", "POPI", "PUSH", "POP", "RETN", "SWAP", "OUTI", "OUTC", "HALT"
        };

        return std::find(names.begin(), names.end(), op) != names.end();
    }

    void firstPass() {
        // Cada instrução ou .word ocupa uma palavra; .org avança o endereço.
        std::uint32_t address = 0;

        for (const auto& source : lines) {
            const auto comment = source.text.find(';');
            const std::string text = source.text.substr(0, comment);
            std::size_t pos = 0;

            skipSpace(text, pos);

            if (pos == text.size())
                continue;

            const std::size_t first = pos;

            if (identifierStart(text[pos])) {
                while (pos < text.size() && identifierPart(text[pos]))
                    ++pos;

                if (pos < text.size() && text[pos] == ':') {
                    const auto label = text.substr(first, pos - first);

                    if (address >= ramWords)
                        error(source.number, first + 1, "rótulo fora da RAM");

                    if (!symbols.emplace(label, address).second)
                        error(source.number, first + 1, "rótulo duplicado: " + label);

                    ++pos;

                    skipSpace(text, pos);

                    if (pos == text.size())
                        continue;
                } else {
                    pos = first;
                }
            }

            const std::size_t operationColumn = pos + 1;

            if (text[pos] == '.')
                ++pos;

            if (pos == text.size() || !identifierStart(text[pos]))
                error(source.number, operationColumn, "instrução ou diretiva esperada");

            while (pos < text.size() && identifierPart(text[pos]))
                ++pos;

            const auto operation = upper(text.substr(operationColumn - 1, pos - (operationColumn - 1)));

            if (!needsOperand(operation) && !noOperand(operation))
                error(source.number, operationColumn, "instrução desconhecida: " + operation);

            if (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
                error(source.number, pos + 1, "espaço esperado após a instrução");

            skipSpace(text, pos);

            const std::size_t operandColumn = pos + 1;
            const std::size_t operandStart = pos;

            while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
                ++pos;

            const auto operand = text.substr(operandStart, pos - operandStart);

            skipSpace(text, pos);

            if (pos < text.size())
                error(source.number, pos + 1, "operando extra");

            if (needsOperand(operation) && operand.empty())
                error(source.number, operandColumn, "operando obrigatório ausente");

            if (noOperand(operation) && !operand.empty())
                error(source.number, operandColumn, "instrução não aceita operando");

            if (operation == ".ORG") {
                const auto target = number(operand, source.number, operandColumn);

                if (target < address || target >= ramWords)
                    error(source.number, operandColumn, ".org deve avançar até um endereço da RAM");

                address = static_cast<std::uint32_t>(target);
                imageWords = std::max(imageWords, address);

                continue;
            }

            if (address >= ramWords)
                error(source.number, operationColumn, "programa excede 4096 palavras");

            statements.push_back({address, source.number, operationColumn, operation, operand, operandColumn, source.text});
            ++address;

            imageWords = std::max(imageWords, address);
        }
    }

    std::uint32_t encode(const Statement& statement) const {
        const auto& op = statement.operation;

        // Atalhos de E/S: STOD nos endereços mapeados para saída e parada.
        if (op == "OUTI") return 0x10000FFFu;
        if (op == "OUTC") return 0x10000FFEu;
        if (op == "HALT") return 0x10000FFDu;
        if (op == ".WORD") {
            const auto value = operandValue(statement);

            if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::uint32_t>::max())
                error(statement.line, statement.operandColumn, ".word deve caber em 32 bits");

            return static_cast<std::uint32_t>(value);
        }

        static const std::array<std::string_view, 15> formatA = {
            "LODD", "STOD", "ADDD", "SUBD", "JPOS", "JZER", "JUMP", "LOCO",
            "LODL", "STOL", "ADDL", "SUBL", "JNEG", "JNZE", "CALL"
        };

        const auto a = std::find(formatA.begin(), formatA.end(), op);

        if (a != formatA.end()) {
            // Formato A: opcode nos quatro bits altos e campo x de 12 bits.
            const auto value = operandValue(statement);

            if (value < 0 || value > 4095)
                error(statement.line, statement.operandColumn, "campo x fora da faixa 0..4095");

            return (static_cast<std::uint32_t>(a - formatA.begin()) << 28) | static_cast<std::uint32_t>(value);
        }

        static const std::array<std::string_view, 8> formatB = {
            "PSHI", "POPI", "PUSH", "POP", "RETN", "SWAP", "INSP", "DESP"
        };

        const auto b = std::find(formatB.begin(), formatB.end(), op);

        // Formato B: subopcode par; INSP e DESP também usam o campo y.
        const auto subOpcode = static_cast<std::uint32_t>(2 * (b - formatB.begin()));

        std::uint32_t value = 0;

        if (op == "INSP" || op == "DESP") {
            const auto operand = operandValue(statement);

            if (operand < 0 || operand > 255)
                error(statement.line, statement.operandColumn, "campo y fora da faixa 0..255");

            value = static_cast<std::uint32_t>(operand);
        }

        return 0xF0000000u | (subOpcode << 24) | value;
    }

    Program secondPass() const {
        Program result;

        // Os endereços pulados por .org são preenchidos com palavras zero.
        result.words.resize(imageWords, 0);
        result.sources.resize(imageWords, "; preenchimento de .org");
        result.lines.resize(imageWords, 0);

        for (const auto& statement : statements) {
            result.words[statement.address] = encode(statement);
            result.sources[statement.address] = statement.source;
            result.lines[statement.address] = statement.line;
        }

        return result;
    }
};

void writeProgram(const Program& program, const std::filesystem::path& output, const std::filesystem::path& listing) {
    std::ofstream binary(output, std::ios::binary);

    if (!binary)
        throw std::runtime_error("não foi possível criar: " + output.string());

    // O simulador lê cada palavra de 32 bits em ordem little endian.
    for (const auto word : program.words) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            binary.put(static_cast<char>((word >> shift) & 0xFFu));
    }

    if (!binary)
        throw std::runtime_error("erro ao gravar: " + output.string());

    binary.close();

    if (!binary)
        throw std::runtime_error("erro ao fechar: " + output.string());

    if (listing.empty())
        return;

    std::ofstream text(listing);

    if (!text)
        throw std::runtime_error("não foi possível criar: " + listing.string());

    text << "; endereço  palavra   linha  fonte\n";

    for (std::size_t i = 0; i < program.words.size(); ++i) {
        text << std::uppercase << std::hex << std::setfill('0')
             << std::setw(3) << i << "  " << std::setw(8) << program.words[i]
             << std::dec << std::setfill(' ') << "  " << std::setw(4) << program.lines[i]
             << "  " << program.sources[i] << '\n';
    }

    if (!text)
        throw std::runtime_error("erro ao gravar: " + listing.string());
}

void usage() {
    std::cerr << "Uso: assembler programa.asm [-o saida.bin] [-l saida.lst]\n";
}

bool sameFile(const std::filesystem::path& left, const std::filesystem::path& right) {
    // Impede que a saída sobrescreva a entrada ou a própria listagem.
    if (std::filesystem::absolute(left).lexically_normal() == std::filesystem::absolute(right).lexically_normal())
        return true;

    std::error_code error;

    return std::filesystem::equivalent(left, right, error) && !error;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 2;
    }

    std::filesystem::path input, output, listing;

    input = argv[1];

    bool outputSeen = false, listingSeen = false;

    // Aceita as opções de saída em qualquer ordem, uma vez cada.
    for (int i = 2; i < argc; ++i) {
        const std::string_view option(argv[i]);

        if ((option != "-o" && option != "-l") || i + 1 >= argc) {
            usage();
            return 2;
        }

        if (option == "-o") {
            if (outputSeen) {
                usage();
                return 2;
            }

            output = argv[++i];
            outputSeen = true;
        } else {
            if (listingSeen) {
                usage();
                return 2;
            }

            listing = argv[++i];
            listingSeen = true;
        }
    }

    if (input.empty()) {
        usage();
        return 2;
    }

    if (!outputSeen)
        output = input.stem().string() + ".bin";

    if (!listingSeen) {
        listing = output;
        listing.replace_extension(".lst");
    }

    if (output.empty() || listing.empty()) {
        usage();
        return 2;
    }

    try {
        if (sameFile(input, output) || sameFile(input, listing) || sameFile(output, listing)) {
            std::cerr << "entrada, binário e listagem devem usar arquivos diferentes\n";
            return 2;
        }

        const auto program = Assembler(input).assemble();
        writeProgram(program, output, listing);

        std::cout << "assembled_words=" << program.words.size() << " output=" << output.string() << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
