#pragma once

#include <raylib.h>

inline int runGUI() {
    InitWindow(960, 600, "MIC-1 Simulator");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
