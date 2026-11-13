#include "GuiApp.hpp"
#include "SimulatorClient.hpp"
#include "StackPanel.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t maxProgramWords = 4096;
constexpr std::size_t chunkWords = 64;

bool loadProgram(SimulatorClient& client, const std::string& path) {
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return false;

    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(file),
                                         std::istreambuf_iterator<char>()};

    if (file.bad() || data.size() % 4 != 0 || data.size() > maxProgramWords * 4)
        return false;

    std::vector<std::uint8_t> payload;
    tx::ByteWriter begin(payload);

    begin.u32(0);
    begin.u32(static_cast<std::uint32_t>(data.size() / 4));

    if (!client.send(tx::raw(tx::CommandType::LoadBegin), payload))
        return false;

    for (std::size_t offset = 0; offset < data.size() / 4; offset += chunkWords) {
        payload.clear();

        tx::ByteWriter chunk(payload);
        chunk.u32(static_cast<std::uint32_t>(offset));

        const std::size_t end = std::min(offset + chunkWords, data.size() / 4) * 4;
        payload.insert(payload.end(), data.begin() + offset * 4, data.begin() + end);

        if (!client.send(tx::raw(tx::CommandType::LoadData), payload))
            return false;
    }

    return client.send(tx::raw(tx::CommandType::LoadEnd));
}

class GuiApp {
public:
    GuiApp(const std::string& simulator, std::string program)
        : client(simulator), program(std::move(program)) {
        InitWindow(1240, 760, "MIC-3 | GUI");

        std::vector<int> glyphs;

        for (int code = 32; code <= 255; ++code)
            glyphs.push_back(code);

        font = LoadFontEx("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24,
                          glyphs.data(), static_cast<int>(glyphs.size()));

        customFont = font.texture.id != 0;

        if (!customFont)
            font = GetFontDefault();

        SetTargetFPS(60);
    }

    int run() {
        while (!WindowShouldClose()) {
            receiveFrames();
            draw();
        }

        if (customFont)
            UnloadFont(font);

        CloseWindow();

        return 0;
    }

private:
    void drawText(const char* value, int x, int y, int size, Color color) const {
        DrawTextEx(font, value, Vector2{static_cast<float>(x), static_cast<float>(y)},
                   static_cast<float>(size), 1.0f, color);
    }

    bool button(const char* label, int x, int y, int width, int key) const {
        const Rectangle area{static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(width), 42};

        const bool hovered = CheckCollisionPointRec(GetMousePosition(), area);

        DrawRectangleRec(area, hovered ? Color{65, 95, 145, 255} : Color{43, 59, 86, 255});
        drawText(label, x + 12, y + 11, 20, RAYWHITE);

        return IsKeyPressed(key) || (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    }

    void receiveFrames() {
        for (const auto& frame : client.takeFrames()) {
            tx::ByteReader reader(frame.payload.data(), frame.payload.size());

            switch (static_cast<tx::MessageType>(frame.type)) {
                case tx::MessageType::Ack: {
                    const auto command = reader.u8();
                    const auto code = reader.u8();

                    status = "ACK " + std::to_string(command) + " status=" + std::to_string(code);

                    if (code == 0 && (command == tx::raw(tx::CommandType::LoadEnd) ||
                                      command == tx::raw(tx::CommandType::Reset))) {
                        output.clear();
                        outputEvents.clear();
                        cycle = 0;
                        halted = false;
                        stackPanel.reset();
                    }

                    break;
                }
                case tx::MessageType::CycleState:
                    cycle = reader.u32();
                    mpc = reader.u16();
                    break;
                case tx::MessageType::ProgramOut: {
                    const auto kind = reader.u8();
                    const auto value = reader.u32();
                    const std::string piece = kind == 0
                        ? std::to_string(static_cast<std::int32_t>(value)) + "\n"
                        : std::string(1, static_cast<char>(value & 0xFF));

                    outputEvents.push_back(piece);
                    output += piece;

                    break;
                }
                case tx::MessageType::Rewound: {
                    const auto count = reader.u32();

                    if (count <= outputEvents.size())
                        outputEvents.resize(count);

                    output.clear();

                    for (const auto& piece : outputEvents)
                        output += piece;

                    halted = false;
                    status = "Voltou uma instrução";
                    stackPanel.markRewind();
                    break;
                }
                case tx::MessageType::StackState:
                    stackPanel.update(frame);
                    break;
                case tx::MessageType::Halted:
                    halted = true;
                    status = "Programa encerrado (HALTED)";
                    break;
                case tx::MessageType::RunStopped:
                    running = false;
                    status = "RUN_STOPPED reason=" + std::to_string(reader.u8());
                    break;
                case tx::MessageType::Error:
                    status = "ERROR status=" + std::to_string(reader.u8()) +
                             " type=" + std::to_string(reader.u8());
                    break;
                default:
                    break;
            }
        }

        if (!client.connected())
            status = "Simulador desconectado";
    }

    void draw() {
        BeginDrawing();

        ClearBackground(Color{22, 28, 40, 255});

        drawText("MIC-3", 24, 20, 32, RAYWHITE);
        drawText(program.c_str(), 24, 62, 18, LIGHTGRAY);

        if (button("L Carregar", 24, 105, 148, KEY_L))
            status = loadProgram(client, program) ? "Carregando..." : "Falha ao carregar .bin";

        if (button("Espaço Ciclo", 184, 105, 166, KEY_SPACE))
            client.send(tx::raw(tx::CommandType::StepCycle));

        if (button("I Instrução", 362, 105, 162, KEY_I))
            client.send(tx::raw(tx::CommandType::StepInstr));

        if (button(running ? "R Executando" : "R Executar", 536, 105, 150, KEY_R) && !running) {
            std::vector<std::uint8_t> payload;
            tx::ByteWriter writer(payload);

            writer.u32(1000000);
            writer.u8(static_cast<std::uint8_t>(tx::ReportMode::Compact));
            writer.u16(100);

            running = client.send(tx::raw(tx::CommandType::Run), payload);
        }

        if (button("P Pausar", 698, 105, 150, KEY_P))
            client.send(tx::raw(tx::CommandType::Pause));

        if (button("X Reiniciar", 24, 159, 170, KEY_X))
            client.send(tx::raw(tx::CommandType::Reset));

        if (button("B Voltar instrução", 206, 159, 218, KEY_B))
            client.send(tx::raw(tx::CommandType::StepBack));

        drawText(status.c_str(), 24, 220, 20, halted ? GREEN : LIGHTGRAY);
        const std::string state = "Ciclo: " + std::to_string(cycle) + "    MPC: " +
                                  std::to_string(mpc) + (running ? "    RUN" : "");

        drawText(state.c_str(), 24, 260, 22, RAYWHITE);
        drawText("Saída do programa", 24, 315, 22, SKYBLUE);

        DrawRectangle(24, 348, 824, 375, Color{11, 16, 25, 255});

        const std::size_t start = output.size() > 1200 ? output.size() - 1200 : 0;
        drawText(output.substr(start).c_str(), 36, 360, 20, RAYWHITE);
        stackPanel.draw(font);

        EndDrawing();
    }

    SimulatorClient client;
    std::string program;
    Font font{};

    bool customFont = false;

    std::string status = "Conectado. Pressione L para carregar o programa.";
    std::string output;
    std::vector<std::string> outputEvents;
    StackPanel stackPanel;
    std::uint32_t cycle = 0;
    std::uint16_t mpc = 0;

    bool running = false;
    bool halted = false;
};

}  // namespace

int runGUI(const std::string& simulator, const std::string& program) {
    return GuiApp(simulator, program).run();
}
