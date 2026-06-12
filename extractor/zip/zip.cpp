#include <functional>
#include "zip.hpp"
#include <cstring>
#include <fstream>

namespace fve {

namespace {

struct Bits {
    const uint8_t* p;
    size_t n;
    size_t pos = 0;
    uint32_t buf = 0;
    int cnt = 0;
    bool bad = false;

    int bit() {
        if (cnt == 0) {
            if (pos >= n) { bad = true; return 0; }
            buf = p[pos++];
            cnt = 8;
        }
        int b = buf & 1;
        buf >>= 1;
        cnt--;
        return b;
    }
    uint32_t bits(int k) {
        uint32_t v = 0;
        for (int i = 0; i < k; i++) v |= (uint32_t)bit() << i;
        return v;
    }
    void align() { cnt = 0; }
};

struct Huff {
    std::vector<int> count;
    std::vector<int> sym;
    void build(const std::vector<int>& lens, int n) {
        int maxbits = 0;
        for (int i = 0; i < n; i++) if (lens[i] > maxbits) maxbits = lens[i];
        count.assign(maxbits + 1, 0);
        for (int i = 0; i < n; i++) count[lens[i]]++;
        count[0] = 0;
        std::vector<int> offs(maxbits + 2, 0);
        for (int i = 1; i <= maxbits; i++) offs[i + 1] = offs[i] + count[i];
        sym.assign(n, 0);
        for (int i = 0; i < n; i++)
            if (lens[i]) sym[offs[lens[i]]++] = i;
        maxbits_ = maxbits;
    }
    int decode(Bits& b) const {
        int code = 0, first = 0, idx = 0;
        for (int len = 1; len <= maxbits_; len++) {
            code |= b.bit();
            int c = count[len];
            if (code - first < c) return sym[idx + (code - first)];
            idx += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
    int maxbits_ = 0;
};

const int LBASE[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                     35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int LEXT[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int DBASE[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                     193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                     6145, 8193, 12289, 16385, 24577};
const int DEXT[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void fixed_huff(Huff& lit, Huff& dist) {
    std::vector<int> ll(288);
    for (int i = 0; i < 144; i++) ll[i] = 8;
    for (int i = 144; i < 256; i++) ll[i] = 9;
    for (int i = 256; i < 280; i++) ll[i] = 7;
    for (int i = 280; i < 288; i++) ll[i] = 8;
    lit.build(ll, 288);
    std::vector<int> dl(30, 5);
    dist.build(dl, 30);
}

bool dynamic_huff(Bits& b, Huff& lit, Huff& dist) {
    int hlit = b.bits(5) + 257;
    int hdist = b.bits(5) + 1;
    int hclen = b.bits(4) + 4;
    static const int ord[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    std::vector<int> cl(19, 0);
    for (int i = 0; i < hclen; i++) cl[ord[i]] = b.bits(3);
    Huff clh;
    clh.build(cl, 19);
    std::vector<int> lens;
    while ((int)lens.size() < hlit + hdist) {
        int s = clh.decode(b);
        if (s < 0) return false;
        if (s < 16) lens.push_back(s);
        else if (s == 16) {
            int r = b.bits(2) + 3;
            if (lens.empty()) return false;
            int prev = lens.back();
            while (r--) lens.push_back(prev);
        } else if (s == 17) {
            int r = b.bits(3) + 3;
            while (r--) lens.push_back(0);
        } else {
            int r = b.bits(7) + 11;
            while (r--) lens.push_back(0);
        }
    }
    std::vector<int> ll(lens.begin(), lens.begin() + hlit);
    std::vector<int> dl(lens.begin() + hlit, lens.begin() + hlit + hdist);
    lit.build(ll, hlit);
    dist.build(dl, hdist);
    return true;
}

uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

}

void inflate_cb(const uint8_t* data, size_t n, const std::function<void(const uint8_t*, size_t)>& sink) {
    Bits b{data, n};
    static const uint32_t WSZ = 32768, MASK = 32767;
    std::vector<uint8_t> window(WSZ);
    uint32_t wpos = 0;
    uint64_t produced = 0;
    uint8_t chunk[65536];
    size_t cn = 0;
    auto flush = [&]() { if (cn) { sink(chunk, cn); cn = 0; } };
    auto emit = [&](uint8_t by) {
        window[wpos] = by;
        wpos = (wpos + 1) & MASK;
        produced++;
        chunk[cn++] = by;
        if (cn == sizeof(chunk)) flush();
    };
    while (!b.bad) {
        int last = b.bit();
        int type = b.bits(2);
        if (type == 0) {
            b.align();
            if (b.pos + 4 > b.n) break;
            uint16_t len = rd16(b.p + b.pos);
            b.pos += 4;
            for (int i = 0; i < len && b.pos < b.n; i++) emit(b.p[b.pos++]);
        } else if (type == 1 || type == 2) {
            Huff lit, dist;
            if (type == 1) fixed_huff(lit, dist);
            else if (!dynamic_huff(b, lit, dist)) break;
            while (true) {
                int s = lit.decode(b);
                if (s < 0) { b.bad = true; break; }
                if (s == 256) break;
                if (s < 256) {
                    emit((uint8_t)s);
                } else {
                    s -= 257;
                    if (s >= 29) { b.bad = true; break; }
                    int len = LBASE[s] + b.bits(LEXT[s]);
                    int ds = dist.decode(b);
                    if (ds < 0 || ds >= 30) { b.bad = true; break; }
                    int d = DBASE[ds] + b.bits(DEXT[ds]);
                    if ((uint64_t)d > produced) { b.bad = true; break; }
                    for (int i = 0; i < len; i++) emit(window[(wpos - d) & MASK]);
                }
            }
        } else {
            break;
        }
        if (last) break;
    }
    flush();
}

std::vector<uint8_t> inflate(const uint8_t* data, size_t n, size_t hint) {
    std::vector<uint8_t> out;
    if (hint) out.reserve(hint);
    inflate_cb(data, n, [&](const uint8_t* p, size_t m) { out.insert(out.end(), p, p + m); });
    return out;
}

Zip read_zip(const std::vector<uint8_t>& buf) {
    Zip z;
    const uint8_t* p = buf.data();
    size_t n = buf.size();
    if (n < 22) return z;
    size_t eocd = n - 22;
    while (eocd > 0 && rd32(p + eocd) != 0x06054b50) eocd--;
    if (rd32(p + eocd) != 0x06054b50) return z;
    uint16_t total = rd16(p + eocd + 10);
    uint32_t cd = rd32(p + eocd + 16);
    size_t off = cd;
    for (int i = 0; i < total && off + 46 <= n; i++) {
        if (rd32(p + off) != 0x02014b50) break;
        uint16_t method = rd16(p + off + 10);
        uint32_t csize = rd32(p + off + 20);
        uint32_t usize = rd32(p + off + 24);
        uint16_t fnlen = rd16(p + off + 28);
        uint16_t extra = rd16(p + off + 30);
        uint16_t comment = rd16(p + off + 32);
        uint32_t lho = rd32(p + off + 42);
        std::string name((const char*)(p + off + 46), fnlen);
        if (lho + 30 <= n) {
            uint16_t lfn = rd16(p + lho + 26);
            uint16_t lextra = rd16(p + lho + 28);
            size_t ds = lho + 30 + lfn + lextra;
            if (method == 0) {
                z.entries[name] = std::vector<uint8_t>(p + ds, p + ds + usize);
            } else if (method == 8) {
                z.entries[name] = inflate(p + ds, csize, usize);
            }
        }
        off += 46 + fnlen + extra + comment;
    }
    z.ok = true;
    return z;
}

Zip read_zip_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return Zip{};
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return read_zip(buf);
}

std::string Zip::text(const std::string& name) const {
    auto it = entries.find(name);
    if (it == entries.end()) return "";
    return std::string(it->second.begin(), it->second.end());
}

const std::vector<uint8_t>* Zip::bytes(const std::string& name) const {
    auto it = entries.find(name);
    return it == entries.end() ? nullptr : &it->second;
}

std::vector<std::string> Zip::list() const {
    std::vector<std::string> v;
    for (const auto& kv : entries) v.push_back(kv.first);
    return v;
}

bool ZipIndex::has(const std::string& name) const {
    for (const auto& e : entries) if (e.name == name) return true;
    return false;
}

uint32_t ZipIndex::usize_of(const std::string& name) const {
    for (const auto& e : entries) if (e.name == name) return e.usize;
    return 0;
}

std::vector<std::string> ZipIndex::names() const {
    std::vector<std::string> v;
    for (const auto& e : entries) v.push_back(e.name);
    return v;
}

std::vector<uint8_t> ZipIndex::extract(const std::string& name, size_t max_usize) const {
    std::vector<uint8_t> out;
    const ZipEntry* e = nullptr;
    for (const auto& x : entries) if (x.name == name) { e = &x; break; }
    if (!e) return out;
    if (max_usize && e->usize > max_usize) return out;
    std::ifstream f(path, std::ios::binary);
    if (!f) return out;
    f.seekg(e->lho);
    uint8_t lh[30];
    f.read((char*)lh, 30);
    if (f.gcount() != 30 || rd32(lh) != 0x04034b50) return out;
    uint16_t lfn = rd16(lh + 26);
    uint16_t lextra = rd16(lh + 28);
    f.seekg((std::streamoff)e->lho + 30 + lfn + lextra);
    std::vector<uint8_t> comp(e->csize);
    f.read((char*)comp.data(), e->csize);
    if ((uint32_t)f.gcount() != e->csize) return out;
    if (e->method == 0) return comp;
    if (e->method == 8) return inflate(comp.data(), comp.size(), e->usize);
    return out;
}

std::string ZipIndex::text(const std::string& name, size_t max_usize) const {
    auto b = extract(name, max_usize);
    return std::string(b.begin(), b.end());
}

bool ZipIndex::inflate_to(const std::string& name, const std::function<void(const uint8_t*, size_t)>& sink) const {
    const ZipEntry* e = nullptr;
    for (const auto& x : entries) if (x.name == name) { e = &x; break; }
    if (!e) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(e->lho);
    uint8_t lh[30];
    f.read((char*)lh, 30);
    if (f.gcount() != 30 || rd32(lh) != 0x04034b50) return false;
    uint16_t lfn = rd16(lh + 26);
    uint16_t lextra = rd16(lh + 28);
    f.seekg((std::streamoff)e->lho + 30 + lfn + lextra);
    std::vector<uint8_t> comp(e->csize);
    f.read((char*)comp.data(), e->csize);
    if ((uint32_t)f.gcount() != e->csize) return false;
    if (e->method == 0) { sink(comp.data(), comp.size()); return true; }
    if (e->method == 8) { inflate_cb(comp.data(), comp.size(), sink); return true; }
    return false;
}

ZipIndex open_zip_file(const std::string& path) {
    ZipIndex z;
    z.path = path;
    std::ifstream f(path, std::ios::binary);
    if (!f) return z;
    f.seekg(0, std::ios::end);
    long long fsize = f.tellg();
    if (fsize < 22) return z;
    long long tail = fsize < 65557 ? fsize : 65557;
    f.seekg(fsize - tail);
    std::vector<uint8_t> buf(tail);
    f.read((char*)buf.data(), tail);
    long long e = tail - 22;
    while (e > 0 && rd32(buf.data() + e) != 0x06054b50) e--;
    if (rd32(buf.data() + e) != 0x06054b50) return z;
    uint16_t total = rd16(buf.data() + e + 10);
    uint32_t cdsize = rd32(buf.data() + e + 12);
    uint32_t cdoff = rd32(buf.data() + e + 16);
    std::vector<uint8_t> cd(cdsize);
    f.seekg(cdoff);
    f.read((char*)cd.data(), cdsize);
    if ((uint32_t)f.gcount() != cdsize) return z;
    size_t off = 0;
    const uint8_t* p = cd.data();
    for (int i = 0; i < total && off + 46 <= cd.size(); i++) {
        if (rd32(p + off) != 0x02014b50) break;
        ZipEntry en;
        en.method = rd16(p + off + 10);
        en.csize = rd32(p + off + 20);
        en.usize = rd32(p + off + 24);
        uint16_t fnlen = rd16(p + off + 28);
        uint16_t extra = rd16(p + off + 30);
        uint16_t comment = rd16(p + off + 32);
        en.lho = rd32(p + off + 42);
        en.name = std::string((const char*)(p + off + 46), fnlen);
        z.entries.push_back(en);
        off += 46 + fnlen + extra + comment;
    }
    z.ok = true;
    return z;
}

}
