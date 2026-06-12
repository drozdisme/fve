#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fve {

class PbWriter {
public:
    void varint(int field, uint64_t v);
    void boolean(int field, bool v);
    void fixed64(int field, uint64_t v);
    void dbl(int field, double v);
    void str(int field, const std::string& s);
    void bytes(int field, const std::vector<uint8_t>& b);
    void message(int field, const PbWriter& sub);
    const std::string& data() const { return buf_; }

private:
    void key(int field, int wire);
    void raw_varint(uint64_t v);
    void len_prefixed(int field, const char* p, size_t n);
    std::string buf_;
};

struct PbField {
    int field = 0;
    int wire = 0;
    uint64_t v = 0;
    std::string bytes;
};

class PbReader {
public:
    PbReader(const char* p, size_t n) : own_(p, n) {}
    PbReader(const std::string& s) : own_(s) {}
    bool next(PbField& f);
    static double as_double(uint64_t bits);

private:
    uint64_t raw_varint();
    std::string own_;
    size_t i_ = 0;
};

}
