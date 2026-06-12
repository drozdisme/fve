#include "protobuf.hpp"
#include <cstring>

namespace fve {

void PbWriter::raw_varint(uint64_t v) {
    while (v >= 0x80) {
        buf_ += (char)((v & 0x7F) | 0x80);
        v >>= 7;
    }
    buf_ += (char)v;
}

void PbWriter::key(int field, int wire) { raw_varint(((uint64_t)field << 3) | wire); }

void PbWriter::varint(int field, uint64_t v) {
    key(field, 0);
    raw_varint(v);
}

void PbWriter::boolean(int field, bool v) { varint(field, v ? 1 : 0); }

void PbWriter::fixed64(int field, uint64_t v) {
    key(field, 1);
    for (int i = 0; i < 8; i++) buf_ += (char)((v >> (i * 8)) & 0xFF);
}

void PbWriter::dbl(int field, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    fixed64(field, bits);
}

void PbWriter::len_prefixed(int field, const char* p, size_t n) {
    key(field, 2);
    raw_varint(n);
    buf_.append(p, n);
}

void PbWriter::str(int field, const std::string& s) { len_prefixed(field, s.data(), s.size()); }

void PbWriter::bytes(int field, const std::vector<uint8_t>& b) {
    len_prefixed(field, (const char*)b.data(), b.size());
}

void PbWriter::message(int field, const PbWriter& sub) {
    len_prefixed(field, sub.buf_.data(), sub.buf_.size());
}

uint64_t PbReader::raw_varint() {
    uint64_t v = 0;
    int shift = 0;
    while (i_ < own_.size() && shift < 64) {
        uint8_t b = (uint8_t)own_[i_++];
        v |= (uint64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return v;
}

bool PbReader::next(PbField& f) {
    if (i_ >= own_.size()) return false;
    uint64_t k = raw_varint();
    f.field = (int)(k >> 3);
    f.wire = (int)(k & 7);
    f.v = 0;
    f.bytes.clear();
    if (f.wire == 0) {
        f.v = raw_varint();
    } else if (f.wire == 1) {
        for (int j = 0; j < 8 && i_ < own_.size(); j++) f.v |= (uint64_t)(uint8_t)own_[i_++] << (j * 8);
    } else if (f.wire == 2) {
        uint64_t len = raw_varint();
        if (i_ + len > own_.size()) len = own_.size() - i_;
        f.bytes.assign(own_.data() + i_, own_.data() + i_ + len);
        i_ += len;
    } else if (f.wire == 5) {
        for (int j = 0; j < 4 && i_ < own_.size(); j++) f.v |= (uint64_t)(uint8_t)own_[i_++] << (j * 8);
    } else {
        return false;
    }
    return true;
}

double PbReader::as_double(uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
}

}
