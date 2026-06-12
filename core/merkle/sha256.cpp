#include <fstream>
#include "sha256.hpp"

namespace fve {

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t ror(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

void block(uint32_t h[8], const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
               (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
             g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

}

Digest sha256(const uint8_t* data, size_t n) {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t full = n / 64;
    for (size_t i = 0; i < full; i++) block(h, data + i * 64);
    uint8_t tail[128];
    size_t rem = n - full * 64;
    for (size_t i = 0; i < rem; i++) tail[i] = data[full * 64 + i];
    tail[rem] = 0x80;
    size_t pad = (rem < 56) ? 64 : 128;
    for (size_t i = rem + 1; i < pad - 8; i++) tail[i] = 0;
    uint64_t bits = uint64_t(n) * 8;
    for (int i = 0; i < 8; i++) tail[pad - 1 - i] = uint8_t(bits >> (i * 8));
    for (size_t i = 0; i < pad; i += 64) block(h, tail + i);
    Digest out;
    for (int i = 0; i < 8; i++) {
        out[i * 4] = uint8_t(h[i] >> 24);
        out[i * 4 + 1] = uint8_t(h[i] >> 16);
        out[i * 4 + 2] = uint8_t(h[i] >> 8);
        out[i * 4 + 3] = uint8_t(h[i]);
    }
    return out;
}

Digest sha256(const std::string& s) {
    return sha256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

Digest sha256_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t blk[64];
    size_t bl = 0;
    uint64_t total = 0;
    char buf[1 << 16];
    if (f) {
        while (true) {
            f.read(buf, sizeof(buf));
            std::streamsize g = f.gcount();
            if (g <= 0) break;
            for (std::streamsize i = 0; i < g; i++) {
                blk[bl++] = (uint8_t)buf[i];
                total++;
                if (bl == 64) { block(h, blk); bl = 0; }
            }
        }
    }
    uint8_t tail[128];
    for (size_t i = 0; i < bl; i++) tail[i] = blk[i];
    tail[bl] = 0x80;
    size_t pad = (bl < 56) ? 64 : 128;
    for (size_t i = bl + 1; i < pad - 8; i++) tail[i] = 0;
    uint64_t bits = total * 8;
    for (int i = 0; i < 8; i++) tail[pad - 1 - i] = uint8_t(bits >> (i * 8));
    for (size_t i = 0; i < pad; i += 64) block(h, tail + i);
    Digest out;
    for (int i = 0; i < 8; i++) {
        out[i * 4] = uint8_t(h[i] >> 24);
        out[i * 4 + 1] = uint8_t(h[i] >> 16);
        out[i * 4 + 2] = uint8_t(h[i] >> 8);
        out[i * 4 + 3] = uint8_t(h[i]);
    }
    return out;
}

Digest sha256(const std::vector<uint8_t>& v) { return sha256(v.data(), v.size()); }

std::string hex(const Digest& d) {
    static const char* t = "0123456789abcdef";
    std::string s(64, '0');
    for (int i = 0; i < 32; i++) {
        s[i * 2] = t[d[i] >> 4];
        s[i * 2 + 1] = t[d[i] & 0xf];
    }
    return s;
}

Digest from_hex(const std::string& s) {
    Digest d{};
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (int i = 0; i < 32 && (size_t)(i * 2 + 1) < s.size(); i++)
        d[i] = uint8_t((nib(s[i * 2]) << 4) | nib(s[i * 2 + 1]));
    return d;
}

}
