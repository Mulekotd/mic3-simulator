#pragma once

#include "io/Frame.hpp"

#include <raylib.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class StackPanel {
public:
    void reset();
    void markRewind();
    void update(const tx::Frame& frame);
    void draw(Font font);

private:
    void addEvent(const std::string& event);

    std::uint32_t cycle = 0;
    std::uint32_t sp = 0xFFC;
    std::uint16_t depth = 0;
    std::vector<std::uint32_t> words;
    std::deque<std::string> events;
    int scroll = 0;
    bool hasState = false;
    bool restoring = false;
};
