#include "StackPanel.hpp"

#include "io/Protocol.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

constexpr int panelX = 875;
constexpr int panelY = 20;
constexpr int panelWidth = 340;
constexpr int panelHeight = 720;
constexpr int listRows = 13;

std::string hexWord(std::uint32_t value, int digits) {
    std::ostringstream text;
    text << "0x" << std::uppercase << std::hex << std::setw(digits)
         << std::setfill('0') << value;
    return text.str();
}

void drawLabel(Font font, const std::string& value, int x, int y, int size, Color color) {
    DrawTextEx(font, value.c_str(), Vector2{static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(size), 1.0f, color);
}

}  // namespace

void StackPanel::reset() {
    cycle = 0;
    sp = 0xFFC;
    depth = 0;
    words.clear();
    events.clear();
    scroll = 0;
    hasState = false;
    restoring = false;
}

void StackPanel::markRewind() {
    restoring = true;
}

void StackPanel::addEvent(const std::string& event) {
    if (events.size() == 5)
        events.pop_front();
    events.push_back(event);
}

void StackPanel::update(const tx::Frame& frame) {
    tx::ByteReader reader(frame.payload.data(), frame.payload.size());
    const auto nextCycle = reader.u32();
    const auto nextSp = reader.u32();
    const auto nextDepth = reader.u16();
    const auto count = reader.u8();

    if (count > 32 || frame.payload.size() != 11 + 4 * count || count > nextDepth)
        throw std::runtime_error("STACK_STATE inválido");

    std::vector<std::uint32_t> nextWords;
    nextWords.reserve(count);
    for (unsigned i = 0; i < count; ++i)
        nextWords.push_back(reader.u32());

    if (hasState && restoring) {
        addEvent("Estado anterior restaurado");
    } else if (hasState) {
        if (nextSp < sp) {
            addEvent("ciclo " + std::to_string(nextCycle) + ": nova posição " +
                     hexWord(nextSp, 3));
        } else if (nextSp > sp) {
            const std::size_t removed = std::min<std::size_t>(nextSp - sp, words.size());
            for (std::size_t i = 0; i < removed; ++i) {
                addEvent("Retirado " + hexWord(sp + static_cast<std::uint32_t>(i), 3) +
                         " = " + hexWord(words[i], 8));
            }
            if (removed == 0)
                addEvent("ciclo " + std::to_string(nextCycle) + ": SP subiu");
        }

        for (std::size_t i = 0; i < nextWords.size(); ++i) {
            const std::uint32_t address = nextSp + static_cast<std::uint32_t>(i);
            if (address < sp || address - sp >= words.size() ||
                words[address - sp] == nextWords[i])
                continue;

            addEvent(hexWord(address, 3) + " <- " + hexWord(nextWords[i], 8));
        }
    }

    cycle = nextCycle;
    sp = nextSp;
    depth = nextDepth;
    words = std::move(nextWords);
    hasState = true;
    restoring = false;
    scroll = 0;
}

void StackPanel::draw(Font font) {
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, Color{31, 43, 62, 255});
    DrawRectangleLinesEx(Rectangle{static_cast<float>(panelX), static_cast<float>(panelY),
                                   static_cast<float>(panelWidth), static_cast<float>(panelHeight)},
                         1.0f, Color{80, 103, 136, 255});

    drawLabel(font, "Pilha de execução", panelX + 16, panelY + 14, 25, RAYWHITE);
    drawLabel(font, "SP: " + hexWord(sp, 3), panelX + 16, panelY + 57, 19, SKYBLUE);
    drawLabel(font, "Profundidade: " + std::to_string(depth) + " palavras",
              panelX + 16, panelY + 86, 17, LIGHTGRAY);

    const Rectangle listArea{static_cast<float>(panelX + 15), static_cast<float>(panelY + 132),
                             static_cast<float>(panelWidth - 30), 365.0f};
    DrawRectangleRec(listArea, Color{17, 26, 40, 255});

    if (CheckCollisionPointRec(GetMousePosition(), listArea)) {
        const int maxScroll = std::max(0, static_cast<int>(words.size()) - listRows);
        scroll = std::clamp(scroll - static_cast<int>(GetMouseWheelMove()), 0, maxScroll);
    }

    if (!hasState) {
        drawLabel(font, "Carregue um programa.", panelX + 25, panelY + 160, 17, LIGHTGRAY);
    } else if (sp > 0xFFC) {
        drawLabel(font, "SP fora da região da pilha", panelX + 25, panelY + 160, 16, ORANGE);
    } else if (depth == 0) {
        drawLabel(font, "Pilha vazia", panelX + 25, panelY + 160, 17, LIGHTGRAY);
    } else {
        const std::size_t end = std::min(words.size(), static_cast<std::size_t>(scroll + listRows));
        for (std::size_t i = static_cast<std::size_t>(scroll); i < end; ++i) {
            const std::uint32_t address = sp + static_cast<std::uint32_t>(i);
            const std::string row = hexWord(address, 3) + "  " + hexWord(words[i], 8);
            drawLabel(font, row, panelX + 25, panelY + 143 +
                      27 * static_cast<int>(i - scroll), 17, i == 0 ? GREEN : RAYWHITE);
        }
    }

    drawLabel(font, "Mostrando " + std::to_string(words.size()) + " de " +
              std::to_string(depth) + " palavras", panelX + 16, panelY + 512, 15, LIGHTGRAY);
    drawLabel(font, "Alterações recentes", panelX + 16, panelY + 547, 19, SKYBLUE);

    int eventY = panelY + 579;
    for (const auto& event : events) {
        drawLabel(font, event, panelX + 16, eventY, 15, LIGHTGRAY);
        eventY += 25;
    }
}
