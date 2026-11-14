#include "CLI.hpp"
#include <iostream>
#include <string_view>

#ifdef MIC1_HAS_GUI
#include "GUI.hpp"
#endif

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--gui") {
#ifdef MIC1_HAS_GUI
        return runGUI();
#else
        std::cerr << "GUI indisponível: recompile com MIC1_ENABLE_GUI=ON\n";
        return 1;
#endif
    }

    return runCLI(argc, argv);
}
