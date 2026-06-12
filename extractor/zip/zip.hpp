#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <functional>
#include <vector>

namespace fve {

std::vector<uint8_t> inflate(const uint8_t* data, size_t n, size_t hint = 0);
void inflate_cb(const uint8_t* data, size_t n, const std::function<void(const uint8_t*, size_t)>& sink);

struct Zip {
    std::map<std::string, std::vector<uint8_t>> entries;
    bool ok = false;
    bool has(const std::string& name) const { return entries.count(name) > 0; }
    std::string text(const std::string& name) const;
    const std::vector<uint8_t>* bytes(const std::string& name) const;
    std::vector<std::string> list() const;
};

Zip read_zip(const std::vector<uint8_t>& buf);
Zip read_zip_file(const std::string& path);

struct ZipEntry {
    std::string name;
    uint16_t method = 0;
    uint32_t csize = 0;
    uint32_t usize = 0;
    uint32_t lho = 0;
};

struct ZipIndex {
    std::string path;
    std::vector<ZipEntry> entries;
    bool ok = false;
    bool has(const std::string& name) const;
    uint32_t usize_of(const std::string& name) const;
    std::vector<std::string> names() const;
    std::vector<uint8_t> extract(const std::string& name, size_t max_usize = 0) const;
    std::string text(const std::string& name, size_t max_usize = 0) const;
    bool inflate_to(const std::string& name, const std::function<void(const uint8_t*, size_t)>& sink) const;
};

ZipIndex open_zip_file(const std::string& path);

}
