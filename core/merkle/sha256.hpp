#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fve {

using Digest = std::array<uint8_t, 32>;

Digest sha256(const uint8_t* data, size_t n);
Digest sha256(const std::string& s);
Digest sha256(const std::vector<uint8_t>& v);
Digest sha256_file(const std::string& path);

std::string hex(const Digest& d);
Digest from_hex(const std::string& s);

}
