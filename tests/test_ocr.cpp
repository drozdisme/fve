#include "../extractor/ocr/ocr.hpp"
#include "../extractor/zip/zip.hpp"
#include "framework.hpp"
#include <fstream>
#include <sstream>

using namespace fve;
using namespace fvetest;

void test_ocr() {
    cur = "ocr";
    Zip z = read_zip_file("tests/fixtures/img.xlsx");
    auto imgs = extract_images(z);
    CHECK(imgs.size() == 1);
    CHECK(imgs[0].name == "xl/media/image1.png");
    CHECK(imgs[0].bytes.size() > 8);
    CHECK(imgs[0].bytes[0] == 0x89 && imgs[0].bytes[1] == 0x50);

    bool ok = false;
    Nodep n = lower_symbolic("=a/(b*c)-1", ok);
    CHECK(ok);
    CHECK(n->kind == Kind::Oper);

    Lattice lat;
    lat.slot({{"a", 0.9}});
    lat.slot({{"/", 1.0}});
    lat.slot({{"b", 0.6}, {"d", 0.4}});
    Bundle b = bundle_from_lattice(lat, 8, 0.05);
    CHECK(b.size() == 2);
    Nodep nb = b.lower("h0");
    CHECK(nb->kind == Kind::Disj);
    CHECK(nb->cands.size() == 2);

    Lattice lat2;
    lat2.slot({{"a", 0.9}});
    lat2.slot({{"+", 1.0}});
    lat2.slot({{"?", 0.5}});
    Bundle b2 = bundle_from_lattice(lat2, 8, 0.05);
    Nodep n2 = b2.lower("h1");
    bool has_hole = false;
    if (n2->kind == Kind::Hole) has_hole = true;
    for (auto& c : n2->cands) if (c->kind == Kind::Hole) has_hole = true;
    CHECK(has_hole);

    Lattice lat3;
    lat3.slot({{"x", 0.99}});
    lat3.slot({{"^", 0.99}});
    lat3.slot({{"2", 0.99}});
    Bundle b3 = bundle_from_lattice(lat3, 8, 0.05);
    CHECK(b3.size() == 1);
    CHECK(!b3.ambiguous());

    bool o1 = false;
    lower_symbolic("=SQRT(a)+EXP(b)-LN(c)", o1);
    CHECK(o1);
    bool o2 = false;
    lower_symbolic("=SIN(a)*COS(b)/TAN(c)", o2);
    CHECK(o2);
    bool o3 = false;
    lower_symbolic("=ABS(a)+POWER(b,2)-x%", o3);
    CHECK(o3);
    bool o4 = true;
    lower_symbolic("=VLOOKUP(a,b,c)", o4);
    CHECK(!o4);
    bool o5 = true;
    lower_symbolic("=a+", o5);
    CHECK(!o5);

    Raster rr = render_text("a/(b*c)-1");
    CHECK(rr.h == 7 && rr.w > 0);
    CHECK(segment(rr).size() == 9);
    Lattice lr = lattice_from_raster(rr);
    std::string topr;
    for (auto& s : lr.slots) topr += s.empty() ? '?' : s[0].tok[0];
    CHECK(topr == "a/(b*c)-1");
    Bundle br = bundle_from_lattice(lr, 8, 0.05);
    CHECK(br.size() == 1 && !br.ambiguous());

    std::ifstream pf("tests/fixtures/eq1.png", std::ios::binary);
    std::stringstream ps;
    ps << pf.rdbuf();
    std::string pstr = ps.str();
    std::vector<uint8_t> png(pstr.begin(), pstr.end());
    Raster pr = decode_png(png);
    CHECK(pr.w > 0 && pr.h == 7);
    Lattice lp = lattice_from_raster(pr);
    std::string topp;
    for (auto& s : lp.slots) topp += s.empty() ? '?' : s[0].tok[0];
    CHECK(topp == "a/(b*c)-1");

    std::ifstream pf2("tests/fixtures/eq2.png", std::ios::binary);
    std::stringstream ps2;
    ps2 << pf2.rdbuf();
    std::string p2 = ps2.str();
    std::vector<uint8_t> png2(p2.begin(), p2.end());
    Raster pr2 = decode_png(png2);
    CHECK(pr2.h == 21);
    Lattice lp2 = lattice_from_raster(pr2);
    std::string topp2;
    for (auto& s : lp2.slots) topp2 += s.empty() ? '?' : s[0].tok[0];
    CHECK(topp2 == "(a+b)/c");

    std::vector<uint8_t> notpng = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    Raster bad = decode_png(notpng);
    CHECK(bad.w == 0 && bad.h == 0);
}
