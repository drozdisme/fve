#pragma once
#include "../../model/bundle/bundle.hpp"
#include "../zip/zip.hpp"
#include <string>
#include <vector>

namespace fve {

struct Image {
    std::string name;
    std::vector<uint8_t> bytes;
};

std::vector<Image> extract_images(const Zip& z);

struct Alt {
    std::string tok;
    double conf;
};

struct Lattice {
    std::vector<std::vector<Alt>> slots;
    void slot(std::vector<Alt> a) { slots.push_back(std::move(a)); }
};

struct Raster {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> g;
    uint8_t at(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? 255 : g[y * w + x]; }
};

Raster decode_png(const std::vector<uint8_t>& bytes);
Raster render_text(const std::string& text);

struct GBox {
    int x0, y0, x1, y1;
};

std::vector<GBox> segment(const Raster& r);
Lattice lattice_from_raster(const Raster& r);

Bundle bundle_from_lattice(const Lattice& lat, int max_cands = 8, double floor = 0.05);

Nodep lower_symbolic(const std::string& formula, bool& ok);

}
