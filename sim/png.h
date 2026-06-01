// Minimal self-contained RGB888 PNG writer (no zlib dependency: uses deflate
// "stored" blocks). Enough to dump LVGL framebuffers as artifacts.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace simpng {

inline uint32_t crc32(const uint8_t* p, size_t n, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0xEDB88320u & (~(crc & 1) + 1));
    }
    return crc;
}
inline uint32_t adler32(const uint8_t* p, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; ++i) { a = (a + p[i]) % 65521; b = (b + a) % 65521; }
    return (b << 16) | a;
}
inline void be32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}
inline void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    be32(out, (uint32_t)data.size());
    size_t crc_start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t c = crc32(&out[crc_start], out.size() - crc_start) ^ 0xFFFFFFFFu;
    be32(out, c);
}

// rgb: w*h*3 bytes, row-major, no padding.
inline bool write_rgb(const char* path, const uint8_t* rgb, int w, int h) {
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (1 + w * 3));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);  // filter type 0 (none)
        raw.insert(raw.end(), rgb + (size_t)y * w * 3, rgb + (size_t)(y + 1) * w * 3);
    }
    // zlib stream: 2-byte header + stored deflate blocks + adler32.
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t off = 0;
    while (off < raw.size()) {
        size_t n = raw.size() - off; if (n > 65535) n = 65535;
        bool last = (off + n >= raw.size());
        z.push_back(last ? 1 : 0);             // BFINAL, BTYPE=00 (stored)
        z.push_back(n & 0xFF); z.push_back((n >> 8) & 0xFF);
        uint16_t nlen = ~(uint16_t)n;
        z.push_back(nlen & 0xFF); z.push_back((nlen >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
    }
    be32(z, adler32(raw.data(), raw.size()));

    std::vector<uint8_t> out = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    std::vector<uint8_t> ihdr;
    be32(ihdr, w); be32(ihdr, h);
    ihdr.push_back(8); ihdr.push_back(2);  // 8-bit, colour type 2 (truecolour RGB)
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

}  // namespace simpng
