#include "ocr.hpp"
#include "../zip/zip.hpp"
#include <algorithm>
#include <cstring>

namespace fve {

namespace {

struct Glyph {
    char ch;
    const char* rows[7];
};

const Glyph FONT[] = {
    {'0', {" ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### "}},
    {'1', {"  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### "}},
    {'2', {" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"}},
    {'3', {" ### ", "#   #", "    #", "  ## ", "    #", "#   #", " ### "}},
    {'4', {"   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "}},
    {'5', {"#####", "#    ", "#### ", "    #", "    #", "#   #", " ### "}},
    {'6', {" ### ", "#   #", "#    ", "#### ", "#   #", "#   #", " ### "}},
    {'7', {"#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   "}},
    {'8', {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "}},
    {'9', {" ### ", "#   #", "#   #", " ####", "    #", "#   #", " ### "}},
    {'+', {"     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     "}},
    {'-', {"     ", "     ", "     ", "#####", "     ", "     ", "     "}},
    {'*', {"     ", "# # #", " ### ", "#####", " ### ", "# # #", "     "}},
    {'/', {"    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "}},
    {'(', {"  ## ", " #   ", " #   ", " #   ", " #   ", " #   ", "  ## "}},
    {')', {" ##  ", "   # ", "   # ", "   # ", "   # ", "   # ", " ##  "}},
    {'.', {"     ", "     ", "     ", "     ", "     ", " ##  ", " ##  "}},
    {'^', {"  #  ", " # # ", "#   #", "     ", "     ", "     ", "     "}},
    {'a', {"     ", " ### ", "    #", " ####", "#   #", "#   #", " ####"}},
    {'b', {"#    ", "#    ", "#### ", "#   #", "#   #", "#   #", "#### "}},
    {'c', {"     ", "     ", " ####", "#    ", "#    ", "#    ", " ####"}},
    {'x', {"     ", "     ", "#   #", " # # ", "  #  ", " # # ", "#   #"}},
    {'L', {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"}},
    {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
};
const int NFONT = sizeof(FONT) / sizeof(FONT[0]);

void norm_grid(const Raster& r, const GBox& b, uint8_t out[7][5]) {
    int bw = b.x1 - b.x0 + 1;
    int cx = (b.x0 + b.x1) / 2;
    for (int gy = 0; gy < 7; gy++)
        for (int gx = 0; gx < 5; gx++) {
            int sx = bw <= 5 ? cx - 2 + gx : b.x0 + gx * (bw - 1) / 4;
            int sy = r.h <= 1 ? 0 : gy * (r.h - 1) / 6;
            out[gy][gx] = r.at(sx, sy) < 128 ? 1 : 0;
        }
}

double match(const uint8_t cell[7][5], const Glyph& g) {
    int same = 0;
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 5; x++) {
            int gv = g.rows[y][x] == '#' ? 1 : 0;
            if (cell[y][x] == gv) same++;
        }
    return same / 35.0;
}

}

Raster render_text(const std::string& text) {
    Raster r;
    r.h = 7;
    r.w = (int)text.size() * 6 + 1;
    if (r.w < 1) r.w = 1;
    r.g.assign(r.w * r.h, 255);
    int x = 0;
    for (char ch : text) {
        const Glyph* gp = nullptr;
        for (int i = 0; i < NFONT; i++)
            if (FONT[i].ch == ch) { gp = &FONT[i]; break; }
        if (gp) {
            for (int y = 0; y < 7; y++)
                for (int c = 0; c < 5; c++)
                    if (gp->rows[y][c] == '#') r.g[y * r.w + x + c] = 0;
        }
        x += 6;
    }
    return r;
}

std::vector<GBox> segment(const Raster& r) {
    std::vector<GBox> out;
    int x = 0;
    while (x < r.w) {
        bool ink = false;
        for (int y = 0; y < r.h; y++)
            if (r.at(x, y) < 128) { ink = true; break; }
        if (!ink) { x++; continue; }
        int x0 = x;
        while (x < r.w) {
            bool col = false;
            for (int y = 0; y < r.h; y++)
                if (r.at(x, y) < 128) { col = true; break; }
            if (!col) break;
            x++;
        }
        int x1 = x - 1;
        int y0 = r.h, y1 = -1;
        for (int yy = 0; yy < r.h; yy++)
            for (int xx = x0; xx <= x1; xx++)
                if (r.at(xx, yy) < 128) { y0 = std::min(y0, yy); y1 = std::max(y1, yy); }
        if (y1 < 0) { y0 = 0; y1 = r.h - 1; }
        out.push_back(GBox{x0, y0, x1, y1});
    }
    return out;
}

Lattice lattice_from_raster(const Raster& r) {
    Lattice lat;
    for (const auto& b : segment(r)) {
        uint8_t cell[7][5];
        norm_grid(r, b, cell);
        std::vector<std::pair<double, char>> scored;
        for (int i = 0; i < NFONT; i++) scored.push_back({match(cell, FONT[i]), FONT[i].ch});
        std::sort(scored.begin(), scored.end(), [](auto& a, auto& b2) { return a.first > b2.first; });
        std::vector<Alt> alts;
        double top = scored[0].first;
        for (auto& sc : scored) {
            if (alts.size() >= 3) break;
            if (sc.first >= top - 0.06 && sc.first >= 0.6)
                alts.push_back({std::string(1, sc.second), sc.first});
        }
        if (alts.empty()) alts.push_back({"?", 0.0});
        lat.slot(alts);
    }
    return lat;
}

namespace {

uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

}

Raster decode_png(const std::vector<uint8_t>& d) {
    Raster r;
    static const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (d.size() < 8 || std::memcmp(d.data(), sig, 8) != 0) return r;
    size_t i = 8;
    int w = 0, h = 0, depth = 0, color = 0;
    std::vector<uint8_t> idat;
    while (i + 8 <= d.size()) {
        uint32_t len = be32(&d[i]);
        const uint8_t* type = &d[i + 4];
        const uint8_t* data = &d[i + 8];
        if (i + 12 + len > d.size()) break;
        if (std::memcmp(type, "IHDR", 4) == 0) {
            w = be32(data);
            h = be32(data + 4);
            depth = data[8];
            color = data[9];
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        i += 12 + len;
    }
    if (w <= 0 || h <= 0 || depth != 8 || idat.size() < 3) return r;
    int ch = color == 2 ? 3 : color == 6 ? 4 : color == 4 ? 2 : 1;
    std::vector<uint8_t> raw = inflate(idat.data() + 2, idat.size() - 2, (size_t)(h * (w * ch + 1)));
    size_t stride = (size_t)w * ch;
    if (raw.size() < (stride + 1) * h) return r;
    std::vector<uint8_t> img(stride * h, 0);
    for (int y = 0; y < h; y++) {
        uint8_t f = raw[y * (stride + 1)];
        const uint8_t* src = &raw[y * (stride + 1) + 1];
        uint8_t* cur = &img[y * stride];
        uint8_t* prev = y ? &img[(y - 1) * stride] : nullptr;
        for (size_t x = 0; x < stride; x++) {
            int a = x >= (size_t)ch ? cur[x - ch] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= (size_t)ch) ? prev[x - ch] : 0;
            int v = src[x];
            switch (f) {
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, c); break;
                default: break;
            }
            cur[x] = (uint8_t)(v & 0xFF);
        }
    }
    r.w = w;
    r.h = h;
    r.g.assign((size_t)w * h, 255);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t* px = &img[y * stride + (size_t)x * ch];
            int gray = ch >= 3 ? (px[0] + px[1] + px[2]) / 3 : px[0];
            r.g[y * w + x] = (uint8_t)gray;
        }
    return r;
}

}
