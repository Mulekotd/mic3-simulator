#pragma once

#include "Session.hpp"

#include <raylib.h>

#include <exception>
#include <string>

inline int runGUI(const std::string& initialProgram) {
    Session session;
    std::string message;
    std::string pathInput;

    bool editingPath = false;

    if (!initialProgram.empty()) {
        try {
            session.load(initialProgram);
            session.run();
        } catch (const std::exception& error) {
            message = error.what();
        }
    }

    InitWindow(960, 600, "MIC-1 Simulator");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        try {
            if (editingPath) {
                for (int code; (code = GetCharPressed()) != 0;) {
                    if (code >= 32 && code <= 126 && pathInput.size() < 512)
                        pathInput.push_back(static_cast<char>(code));
                }

                if (IsKeyPressed(KEY_BACKSPACE) && !pathInput.empty())
                    pathInput.pop_back();

                if (IsKeyPressed(KEY_ESCAPE))
                    editingPath = false;

                if (IsKeyPressed(KEY_ENTER) && !pathInput.empty()) {
                    session.load(pathInput);
                    editingPath = false;
                    message = "Programa carregado; pressione R para executar";
                }
            } else {
                if (IsKeyPressed(KEY_R)) session.run();
                if (IsKeyPressed(KEY_P)) session.pause();
                if (IsKeyPressed(KEY_B) && !session.back()) message = "Sem subciclo anterior";
                if (IsKeyPressed(KEY_X)) session.reset();

                if (IsKeyPressed(KEY_SPACE)) {
                    session.pause();
                    session.step();
                }

                if (IsKeyPressed(KEY_L)) {
                    session.pause();
                    pathInput = session.path();
                    editingPath = true;
                }
            }
        } catch (const std::exception& error) {
            session.pause();
            message = error.what();
        }

        if (!editingPath) {
            for (int i = 0; i < 100 && session.isRunning(); ++i) {
                try {
                    session.step();
                } catch (const std::exception& error) {
                    session.pause();
                    message = error.what();
                }
            }
        }

        BeginDrawing();

        ClearBackground(BLACK);

        DrawText("MIC-1 Simulator", 24, 20, 28, RAYWHITE);
        DrawText("R executar  P pausar  Espaco passo  B voltar  X resetar  L carregar", 24, 65, 18, LIGHTGRAY);

        const std::string state = !session.isLoaded() ? "Sem programa" :
            session.isHalted() ? "HALT - aguardando comandos" :
            session.isRunning() ? "Executando" : "Pausado";

        DrawText(state.c_str(), 24, 110, 22, YELLOW);
        DrawText(("Arquivo: " + session.path()).c_str(), 24, 145, 18, RAYWHITE);
        DrawText(("Subciclos: " + std::to_string(session.cycles()) +
                  "   Instrucoes: " + std::to_string(session.instructions())).c_str(),
                 24, 175, 18, RAYWHITE);

        if (const auto* cpu = session.cpu()) {
            DrawText(("PC: " + std::to_string(cpu->bank.read(RegisterBank::PC)) +
                      "   AC: " + std::to_string(cpu->bank.read(RegisterBank::AC)) +
                      "   SP: " + std::to_string(cpu->bank.read(RegisterBank::SP)) +
                      "   MPC: " + std::to_string(cpu->mpc)).c_str(), 24, 205, 18, RAYWHITE);
        }

        if (editingPath) {
            DrawText("Caminho do .bin (Enter carrega, Esc cancela):", 24, 245, 18, LIGHTGRAY);
            DrawText(pathInput.c_str(), 24, 270, 18, YELLOW);
        } else {
            DrawText(message.c_str(), 24, 245, 18, ORANGE);
        }

        if (editingPath)
            DrawText(message.c_str(), 24, 295, 18, ORANGE);

        DrawText("Saida:", 24, 325, 20, LIGHTGRAY);

        const auto& lines = session.outputs();
        const std::size_t first = lines.size() > 12 ? lines.size() - 12 : 0;
        
        for (std::size_t i = first; i < lines.size(); ++i)
            DrawText(lines[i].c_str(), 24, 355 + static_cast<int>(i - first) * 19, 17, RAYWHITE);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
