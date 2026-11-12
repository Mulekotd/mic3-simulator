#include "GuiApp.hpp"

#include <csignal>
#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Uso: mic3_gui caminho/mic3_simulator|unix:///caminho/socket programa.bin\n";
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN);

    try {
        return runGUI(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
