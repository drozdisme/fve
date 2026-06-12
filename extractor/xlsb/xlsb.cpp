#include "xlsb.hpp"
#include "../xml/xml.hpp"
#include <cstring>

namespace fve {

namespace {

enum {
    BrtRowHdr = 0x00,
    BrtCellBlank = 0x01,
    BrtCellRk = 0x02,
    BrtCellError = 0x03,
    BrtCellBool = 0x04,
    BrtCellReal = 0x05,
    BrtCellSt = 0x06,
    BrtCellIsst = 0x07,
    BrtFmlaString = 0x08,
    BrtFmlaNum = 0x09,
    BrtFmlaBool = 0x0A,
    BrtFmlaError = 0x0B,
    BrtSSTItem = 0x13,
    BrtBundleSh = 0x9C
};

uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }

double rd_dbl(const uint8_t* p) {
    double d;
    std::memcpy(&d, p, 8);
    return d;
}

std::string wide_str(const uint8_t* p, size_t n, size_t& used) {
    if (n < 4) { used = n; return ""; }
    uint32_t cch = rd32(p);
    std::string s;
    size_t off = 4;
    for (uint32_t i = 0; i < cch && off + 1 < n; i++) {
        uint16_t ch = rd16(p + off);
        s += (ch < 128) ? (char)ch : '?';
        off += 2;
    }
    used = off;
    return s;
}

const char* fn_name(int ift) {
    switch (ift) {
        case 0: return "COUNT";
        case 4: return "SUM";
        case 5: return "AVERAGE";
        case 6: return "MIN";
        case 7: return "MAX";
        case 24: return "ABS";
        case 19: return "PI";
        case 36: return "AND";
        case 37: return "OR";
        case 38: return "NOT";
        case 1: return "IF";
        default: return nullptr;
    }
}

}

std::vector<Rec> read_records(const std::vector<uint8_t>& bin) {
    std::vector<Rec> out;
    size_t i = 0, n = bin.size();
    const uint8_t* p = bin.data();
    while (i < n) {
        int id = p[i++];
        if (id & 0x80) {
            if (i >= n) break;
            id = (id & 0x7F) | (p[i++] << 7);
        }
        uint32_t size = 0;
        int shift = 0;
        for (int k = 0; k < 4; k++) {
            if (i >= n) break;
            uint8_t b = p[i++];
            size |= (uint32_t)(b & 0x7F) << shift;
            shift += 7;
            if (!(b & 0x80)) break;
        }
        if (i + size > n) break;
        out.push_back({id, p + i, size});
        i += size;
    }
    return out;
}

double rk_num(uint32_t v) {
    bool fint = v & 2, fx100 = v & 1;
    double d;
    if (fint) {
        int32_t iv = (int32_t)v >> 2;
        d = iv;
    } else {
        uint64_t bits = (uint64_t)(v & 0xFFFFFFFC) << 32;
        std::memcpy(&d, &bits, 8);
    }
    return fx100 ? d / 100.0 : d;
}

std::string decode_rpn(const uint8_t* p, size_t n, int, int, bool& ok) {
    ok = true;
    std::vector<std::string> st;
    size_t i = 0;
    auto bin = [&](const char* op) {
        if (st.size() < 2) { ok = false; return; }
        std::string b = st.back(); st.pop_back();
        std::string a = st.back(); st.pop_back();
        st.push_back("(" + a + op + b + ")");
    };
    while (i < n) {
        uint8_t t = p[i++];
        uint8_t base = t & 0x7F;
        if (base == 0x1E) { if (i + 2 > n) { ok = false; break; } st.push_back(std::to_string(rd16(p + i))); i += 2; }
        else if (base == 0x1F) { if (i + 8 > n) { ok = false; break; } char b[32]; snprintf(b, 32, "%g", rd_dbl(p + i)); st.push_back(b); i += 8; }
        else if (base == 0x03) bin("+");
        else if (base == 0x04) bin("-");
        else if (base == 0x05) bin("*");
        else if (base == 0x06) bin("/");
        else if (base == 0x07) bin("^");
        else if (base == 0x12) { }
        else if (base == 0x13) { if (!st.empty()) st.back() = "-" + st.back(); }
        else if (base == 0x15) { }
        else if (base == 0x24 || base == 0x44 || base == 0x64) {
            if (i + 6 > n) { ok = false; break; }
            uint32_t row = rd32(p + i);
            uint16_t colf = rd16(p + i + 4);
            int col = colf & 0x3FFF;
            st.push_back(a1((int)row, col));
            i += 6;
        }
        else if (base == 0x25 || base == 0x45 || base == 0x65) {
            if (i + 12 > n) { ok = false; break; }
            uint32_t r0 = rd32(p + i), r1 = rd32(p + i + 4);
            int c0 = rd16(p + i + 8) & 0x3FFF, c1 = rd16(p + i + 10) & 0x3FFF;
            st.push_back(a1((int)r0, c0) + ":" + a1((int)r1, c1));
            i += 12;
        }
        else if (base == 0x21 || base == 0x41 || base == 0x61) {
            if (i + 2 > n) { ok = false; break; }
            int ift = rd16(p + i); i += 2;
            const char* fn = fn_name(ift);
            if (!fn) { ok = false; break; }
            st.push_back(std::string(fn) + "(" + (st.empty() ? "" : st.back()) + ")");
        }
        else if (base == 0x22 || base == 0x42 || base == 0x62) {
            if (i + 3 > n) { ok = false; break; }
            int cp = p[i]; int ift = rd16(p + i + 1); i += 3;
            const char* fn = fn_name(ift);
            if (!fn || (int)st.size() < cp) { ok = false; break; }
            std::vector<std::string> args(st.end() - cp, st.end());
            st.erase(st.end() - cp, st.end());
            std::string s = std::string(fn) + "(";
            for (int k = 0; k < cp; k++) { if (k) s += ","; s += args[k]; }
            st.push_back(s + ")");
        }
        else { ok = false; break; }
    }
    if (!ok || st.size() != 1) { ok = false; return ""; }
    return st[0];
}

namespace {

std::vector<std::string> sst_bin(const Zip& z) {
    std::vector<std::string> out;
    const auto* b = z.bytes("xl/sharedStrings.bin");
    if (!b) return out;
    for (const auto& r : read_records(*b))
        if (r.id == BrtSSTItem && r.len >= 5) {
            size_t used;
            out.push_back(wide_str(r.data + 1, r.len - 1, used));
        }
    return out;
}

std::vector<std::string> sheet_names(const Zip& z) {
    std::vector<std::string> names;
    const auto* b = z.bytes("xl/workbook.bin");
    if (!b) return names;
    for (const auto& r : read_records(*b))
        if (r.id == BrtBundleSh && r.len >= 12) {
            size_t used;
            std::string rid = wide_str(r.data + 8, r.len - 8, used);
            std::string nm = wide_str(r.data + 8 + used, r.len - 8 - used, used);
            names.push_back(nm);
        }
    return names;
}

void parse_sheet_bin(const std::vector<uint8_t>& bin, Sheet& sh,
                     const std::vector<std::string>& sst) {
    int row = 0;
    for (const auto& r : read_records(bin)) {
        if (r.id == BrtRowHdr && r.len >= 4) {
            row = (int)rd32(r.data);
        } else if (r.len >= 8) {
            int col = (int)rd32(r.data);
            const uint8_t* v = r.data + 8;
            size_t vn = r.len - 8;
            Cell c;
            if (r.id == BrtCellReal && vn >= 8) { c.value = rd_dbl(v); c.has_value = true; }
            else if (r.id == BrtCellRk && vn >= 4) { c.value = rk_num(rd32(v)); c.has_value = true; }
            else if (r.id == BrtCellIsst && vn >= 4) {
                uint32_t idx = rd32(v);
                if (idx < sst.size()) { c.text = sst[idx]; c.has_value = true; }
            }
            else if (r.id == BrtCellSt && vn >= 4) { size_t u; c.text = wide_str(v, vn, u); c.has_value = true; }
            else if (r.id == BrtCellBool && vn >= 1) { c.value = v[0] ? 1 : 0; c.has_value = true; }
            else if (r.id == BrtFmlaNum && vn >= 8) {
                c.value = rd_dbl(v);
                c.has_value = true;
                if (vn >= 8 + 2 + 4) {
                    uint32_t cce = rd32(v + 8 + 2);
                    if (8 + 2 + 4 + cce <= vn) {
                        bool ok;
                        std::string f = decode_rpn(v + 8 + 2 + 4, cce, row, col, ok);
                        if (ok && !f.empty()) c.formula = "=" + f;
                    }
                }
            }
            else continue;
            sh.set(row, col, c);
        }
    }
}

}

LoadResult load_xlsb(const Zip& z) {
    LoadResult res;
    if (!z.ok) { res.err = "bad zip"; return res; }
    res.fmt = "xlsb";
    auto sst = sst_bin(z);
    auto names = sheet_names(z);
    if (names.empty()) {
        for (const auto& nm : z.list())
            if (nm.find("xl/worksheets/sheet") == 0 && nm.size() > 4 && nm.substr(nm.size() - 4) == ".bin")
                names.push_back("Sheet" + std::to_string(names.size() + 1));
    }
    for (size_t i = 0; i < names.size(); i++) {
        Sheet& sh = res.wb.sheet(names[i]);
        std::string path = "xl/worksheets/sheet" + std::to_string(i + 1) + ".bin";
        const auto* b = z.bytes(path);
        if (b) parse_sheet_bin(*b, sh, sst);
    }
    res.ok = true;
    return res;
}

LoadResult load_xlsb_file(const std::string& path) {
    Zip z = read_zip_file(path);
    return load_xlsb(z);
}

namespace {

struct RecAsm {
    std::vector<uint8_t> buf;
    size_t head = 0;
    std::function<void(uint32_t, const uint8_t*, size_t)> on;
    void feed(const uint8_t* p, size_t n) {
        buf.insert(buf.end(), p, p + n);
        parse();
        if (head > (1u << 20)) { buf.erase(buf.begin(), buf.begin() + head); head = 0; }
    }
    void parse() {
        while (true) {
            size_t i = head, n = buf.size();
            if (i >= n) break;
            uint32_t id = buf[i];
            size_t j = i + 1;
            if (id & 0x80) {
                if (j >= n) break;
                id = (id & 0x7F) | ((uint32_t)buf[j] << 7);
                j++;
            }
            uint32_t size = 0;
            int shift = 0;
            bool ok = false;
            size_t k = j;
            for (int c = 0; c < 4; c++) {
                if (k >= n) break;
                uint8_t b = buf[k++];
                size |= (uint32_t)(b & 0x7F) << shift;
                shift += 7;
                if (!(b & 0x80)) { ok = true; break; }
            }
            if (!ok) break;
            if (k + size > n) break;
            on(id, buf.data() + k, size);
            head = k + size;
        }
    }
};

}

void stream_part_records(const ZipIndex& z, const std::string& part,
                         const std::function<void(uint32_t, const uint8_t*, size_t)>& on_rec) {
    RecAsm asm_;
    asm_.on = on_rec;
    z.inflate_to(part, [&](const uint8_t* p, size_t n) { asm_.feed(p, n); });
}

XlsbStats stream_workbook_stats(const std::string& path) {
    XlsbStats st;
    ZipIndex z = open_zip_file(path);
    if (!z.ok) return st;
    for (const auto& name : z.names()) {
        if (name.compare(0, 19, "xl/worksheets/sheet") != 0) continue;
        if (name.find(".bin") == std::string::npos) continue;
        if (name.find("binaryIndex") != std::string::npos) continue;
        st.sheets++;
        long cur_row = -1;
        stream_part_records(z, name, [&](uint32_t id, const uint8_t* p, size_t n) {
            st.records++;
            if (id == BrtRowHdr && n >= 4) {
                cur_row = (long)rd32(p);
                st.rows++;
                if (cur_row > st.max_row) st.max_row = cur_row;
            } else if (id >= BrtCellBlank && id <= BrtFmlaError) {
                st.cells++;
                if (n >= 4) {
                    long col = (long)rd32(p);
                    if (col > st.max_col) st.max_col = col;
                }
            }
        });
        st.bytes += z.usize_of(name);
    }
    return st;
}

}
