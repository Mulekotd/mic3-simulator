#include "CLI.hpp"
#include "io/ProtocolSession.hpp"
#include "io/ProtocolServer.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--protocol")
        return runProtocolSession();

    if (argc == 3 && std::string_view(argv[1]) == "--serve-local") {
        try {
            return runProtocolServer(argv[2]);
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }

    return runCLI(argc, argv);
}
