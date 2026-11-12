#pragma once

#include "Protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace tx {

struct Frame {
    uint8_t type = 0;
    std::vector<uint8_t> payload;
};

// Consome um fluxo de bytes; uma leitura do pipe pode conter parte ou vários quadros.
class FrameDecoder {
public:
    template <typename Callback>

    bool feed(const uint8_t* bytes, std::size_t count, Callback onFrame) {
        for (std::size_t i = 0; i < count; ++i) {
            if (headerSize < kHeaderSize) {
                header[headerSize++] = bytes[i];

                if (headerSize == kHeaderSize) {
                    expected = static_cast<std::size_t>(header[1]) |
                                (static_cast<std::size_t>(header[2]) << 8);

                    if (expected > kMaxPayload) {
                        decoderErrorType = header[0];
                        return false;
                    }

                    if (expected == 0) {
                        onFrame(Frame{header[0], {}});
                        reset();
                    }
                }
            } else {
                payload.push_back(bytes[i]);

                if (payload.size() == expected) {
                    onFrame(Frame{header[0], std::move(payload)});
                    reset();
                }
            }
        }

        return true;
    }

    bool incomplete() const { return headerSize != 0; }
    uint8_t currentType() const { return headerSize ? header[0] : 0; }
    uint8_t errorType() const { return decoderErrorType; }

private:
    void reset() {
        headerSize = 0;
        expected = 0;
        payload.clear();
    }

    uint8_t header[kHeaderSize]{};
    std::size_t headerSize = 0;
    std::size_t expected = 0;
    std::vector<uint8_t> payload;
    uint8_t decoderErrorType = 0;
};

}  // namespace tx
