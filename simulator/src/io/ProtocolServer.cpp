#include "ProtocolServer.hpp"
#include "ProtocolSession.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

class Socket {
public:
    explicit Socket(int descriptor) : descriptor(descriptor) {}
    ~Socket() { if (descriptor >= 0) close(descriptor); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int get() const { return descriptor; }

private:
    int descriptor;
};

}  // namespace

int runProtocolServer(const std::string& socketPath) {
    sockaddr_un address{};
    if (socketPath.empty() || socketPath.size() >= sizeof(address.sun_path))
        throw std::runtime_error("caminho do socket local inválido");

    const Socket listener(socket(AF_UNIX, SOCK_STREAM, 0));
    if (listener.get() < 0)
        throw std::runtime_error("não foi possível criar socket local do simulador");

    struct stat existing{};
    if (lstat(socketPath.c_str(), &existing) == 0) {
        if (!S_ISSOCK(existing.st_mode))
            throw std::runtime_error("o caminho do socket local já contém outro arquivo");
        if (unlink(socketPath.c_str()) < 0)
            throw std::runtime_error("não foi possível remover socket local antigo");
    } else if (errno != ENOENT) {
        throw std::runtime_error("não foi possível verificar socket local");
    }

    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socketPath.c_str(), socketPath.size() + 1);

    if (bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(listener.get(), 8) < 0)
        throw std::runtime_error(std::string("não foi possível abrir socket local: ") + std::strerror(errno));

    if (chmod(socketPath.c_str(), 0666) < 0)
        throw std::runtime_error("não foi possível configurar acesso ao socket local");

    // O processo pai permanece ativo; cada conexão ganha uma sessão isolada.
    std::signal(SIGCHLD, SIG_IGN);
    std::signal(SIGPIPE, SIG_IGN);
    std::cerr << "Simulador aguardando conexões TX/RX em " << socketPath << '\n';

    for (;;) {
        const Socket client(accept(listener.get(), nullptr, nullptr));
        if (client.get() < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("falha ao aceitar conexão TX/RX");
        }

        const pid_t child = fork();
        if (child < 0) {
            std::cerr << "não foi possível iniciar sessão TX/RX\n";
            continue;
        }

        if (child == 0) {
            close(listener.get());
            if (dup2(client.get(), STDIN_FILENO) < 0 ||
                dup2(client.get(), STDOUT_FILENO) < 0)
                _exit(1);
            if (client.get() != STDIN_FILENO && client.get() != STDOUT_FILENO)
                close(client.get());
            _exit(runProtocolSession());
        }
    }
}
