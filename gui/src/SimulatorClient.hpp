#pragma once

#include "io/ProtocolCodec.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <csignal>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

// Canal TX/RX local por pipes ou socket Unix. A thread de leitura nunca acessa a UI.
class SimulatorClient {
public:
    explicit SimulatorClient(const std::string& executable) {
        if (executable.rfind("unix://", 0) == 0) {
            txFd = connectLocal(executable.substr(7));
            rxFd = txFd;
            reader = std::thread([this] { readLoop(); });
            return;
        }

        int toChild[2], fromChild[2];

        if (pipe(toChild) != 0)
            throw std::runtime_error("pipe TX falhou");

        if (pipe(fromChild) != 0) {
            close(toChild[0]);
            close(toChild[1]);
            throw std::runtime_error("pipe RX falhou");
        }

        child = fork();

        if (child < 0) {
            close(toChild[0]);
            close(toChild[1]);
            close(fromChild[0]);
            close(fromChild[1]);
            throw std::runtime_error("fork falhou");
        }

        if (child == 0) {
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);

            close(toChild[0]);
            close(toChild[1]);
            close(fromChild[0]);
            close(fromChild[1]);

            execl(executable.c_str(), executable.c_str(), "--protocol", nullptr);
            _exit(127);
        }

        close(toChild[0]);
        close(fromChild[1]);

        txFd = toChild[1];
        rxFd = fromChild[0];
        reader = std::thread([this] { readLoop(); });
    }

    SimulatorClient(const SimulatorClient&) = delete;
    SimulatorClient& operator=(const SimulatorClient&) = delete;

    ~SimulatorClient() {
        if (txFd == rxFd && txFd >= 0) {
            shutdown(txFd, SHUT_RDWR);
            if (reader.joinable())
                reader.join();
            close(txFd);
            return;
        }

        if (txFd >= 0)
            close(txFd);

        if (child > 0)
            kill(child, SIGTERM);

        if (reader.joinable())
            reader.join();

        if (rxFd >= 0)
            close(rxFd);

        if (child > 0)
            waitpid(child, nullptr, 0);
    }

    bool send(uint8_t type, const std::vector<uint8_t>& payload = {}) {
        const auto bytes = tx::makeFrame(type, payload);

        if (bytes.empty())
            return false;

        std::lock_guard<std::mutex> lock(writeMutex);
        std::size_t offset = 0;

        while (offset < bytes.size()) {
            const ssize_t n = write(txFd, bytes.data() + offset, bytes.size() - offset);

            if (n > 0)
                offset += static_cast<std::size_t>(n);
            else if (n < 0 && errno == EINTR)
                continue;
            else
                return false;
        }

        return true;
    }

    std::vector<tx::Frame> takeFrames() {
        std::lock_guard<std::mutex> lock(readMutex);
        std::vector<tx::Frame> result;

        while (!frames.empty()) {
            result.push_back(std::move(frames.front()));
            frames.pop_front();
        }

        return result;
    }

    bool connected() const { return connectedFlag; }

private:
    static int connectLocal(const std::string& path) {
        sockaddr_un address{};

        if (path.empty() || path.size() >= sizeof(address.sun_path))
            throw std::runtime_error("caminho do socket local inválido");

        address.sun_family = AF_UNIX;

        std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

        int connectedSocket = -1;

        for (int attempt = 0; attempt < 50 && connectedSocket < 0; ++attempt) {
            const int candidate = socket(AF_UNIX, SOCK_STREAM, 0);

            if (candidate < 0)
                throw std::runtime_error("não foi possível criar socket local");

            if (connect(candidate, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
                connectedSocket = candidate;
            } else {
                close(candidate);
            }

            if (connectedSocket < 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (connectedSocket < 0)
            throw std::runtime_error("não foi possível conectar ao socket local do simulador");

        return connectedSocket;
    }

    void readLoop() {
        tx::FrameDecoder decoder;
        uint8_t bytes[4096];

        while (true) {
            const ssize_t count = read(rxFd, bytes, sizeof(bytes));

            if (count > 0) {
                if (!decoder.feed(bytes, static_cast<std::size_t>(count), [this](tx::Frame frame) {
                    std::lock_guard<std::mutex> lock(readMutex);
                    frames.push_back(std::move(frame));
                })) break;
            } else if (count == 0) {
                break;
            } else if (errno != EINTR) {
                break;
            }
        }
        connectedFlag = false;
    }

    pid_t child = -1;
    int txFd = -1;
    int rxFd = -1;
    std::thread reader;
    std::mutex readMutex;
    std::mutex writeMutex;
    std::deque<tx::Frame> frames;
    std::atomic<bool> connectedFlag{true};
};
