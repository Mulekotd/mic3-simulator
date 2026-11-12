#pragma once

#include "generated/ProtocolIds.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tx {

// constantes

constexpr std::size_t kHeaderSize = 3; // TYPE(u8) + LEN(u16)

// bytes little-endian

class ByteWriter {
public:
    explicit ByteWriter(std::vector<uint8_t>& out) : out(out) {}

    void u8(uint8_t v)   { out.push_back(v); }
    void u16(uint16_t v) { le(v, 2); }
    void u32(uint32_t v) { le(v, 4); }
    void u64(uint64_t v) { le(v, 8); }

private:
    void le(uint64_t v, int n) {
        for (int i = 0; i < n; ++i)
            out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }

    std::vector<uint8_t>& out;
};

class ByteReader {
public:
    ByteReader(const uint8_t* data, std::size_t size) : data(data), size(size) {}

    uint8_t  u8()  { return static_cast<uint8_t>(le(1)); }
    uint16_t u16() { return static_cast<uint16_t>(le(2)); }
    uint32_t u32() { return static_cast<uint32_t>(le(4)); }
    uint64_t u64() { return le(8); }

    bool ok() const { return readOk; }
    std::size_t remaining() const { return size - offset; }

private:
    uint64_t le(int n) {
        if (remaining() < static_cast<std::size_t>(n)) {
            readOk = false;
            offset = size;
            return 0;
        }

        uint64_t v = 0;

        for (int i = 0; i < n; ++i)
            v |= static_cast<uint64_t>(data[offset + i]) << (8 * i);

        offset += n;

        return v;
    }

    const uint8_t* data;
    std::size_t size;
    std::size_t offset = 0;
    bool readOk = true;
};

}  // namespace tx
