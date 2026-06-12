#include "../extractor/xlsx/xlsx.hpp"
#include "../oracle/intervene.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

namespace {
bool has_edge(const DepGraph& g, int fr, int fc, int tr, int tc) {
    for (const auto& e : g.edges)
        if (e.from.row == fr && e.from.col == fc && e.to.row == tr && e.to.col == tc) return true;
    return false;
}
}

void test_oracle() {
    cur = "oracle";
    LoadResult lr = load_xlsx_file("tests/fixtures/beam.xlsx");
    Oracle orc(lr.wb);
    orc.build();
    CHECK(orc.inputs().size() == 5);
    CHECK(orc.outputs().size() == 2);

    NEAR(orc.value(0, 3, 1), 2.0, 1e-9);
    NEAR(orc.value(0, 4, 1), 401.0, 1e-9);

    orc.reset();
    orc.set(0, 2, 1, 150);
    NEAR(orc.value(0, 3, 1), 300.0 / 150.0 - 1.0, 1e-9);
    orc.reset();
    NEAR(orc.value(0, 3, 1), 2.0, 1e-9);

    DepGraph g = induce_deps(orc);
    CHECK(g.edges.size() == 6);
    CHECK(has_edge(g, 0, 1, 3, 1));
    CHECK(has_edge(g, 1, 1, 3, 1));
    CHECK(has_edge(g, 2, 1, 3, 1));
    CHECK(has_edge(g, 0, 1, 4, 1));
    CHECK(!has_edge(g, 0, 0, 3, 1));
    for (const auto& e : g.edges)
        if (e.to.row == 3 && e.from.row == 0 && e.from.col == 1)
            NEAR(e.sens, 0.01, 1e-3);

    Workbook wb;
    Sheet& s = wb.sheet("S");
    auto setn = [&](int r, int c, double v) { Cell x; x.has_value = true; x.value = v; s.set(r, c, x); };
    auto setf = [&](int r, int c, const std::string& f) { Cell x; x.formula = f; s.set(r, c, x); };
    setn(0, 0, 8);
    setn(1, 0, 3);
    setn(0, 2, 2);
    setn(1, 2, 5);
    struct FT { int row; std::string f; double exp; };
    std::vector<FT> fts = {
        {2, "=MOD(A1,A2)", 2}, {3, "=INT(2.7)", 2}, {4, "=TRUNC(2.78,1)", 2.7},
        {5, "=SIGN(-5)", -1}, {6, "=FLOOR(7,2)", 6}, {7, "=CEILING(7,2)", 8},
        {8, "=ROUNDUP(2.1,0)", 3}, {9, "=ROUNDDOWN(2.9,0)", 2}, {10, "=PRODUCT(A1,A2)", 24},
        {11, "=SUMSQ(3,4)", 25}, {12, "=DEGREES(PI())", 180}, {13, "=LOG10(100)", 2},
        {14, "=SINH(0)", 0}, {15, "=COSH(0)", 1}, {16, "=TANH(0)", 0},
        {17, "=ACOS(1)", 0}, {18, "=IFERROR(1/0,99)", 99}, {19, "=SUMPRODUCT(A1:A2,C1:C2)", 31},
    };
    for (auto& t : fts) setf(t.row, 1, t.f);
    Oracle o2(wb);
    o2.build();
    for (auto& t : fts) NEAR(o2.value(0, t.row, 1), t.exp, 1e-6);
    NEAR(o2.value(0, 2, 1), 2.0, 1e-9);
    setf(20, 1, "=ATAN2(1,1)");
    setf(21, 1, "=RADIANS(180)");
    Oracle o3(wb);
    o3.build();
    NEAR(o3.value(0, 20, 1), 0.7853981633974483, 1e-9);
    NEAR(o3.value(0, 21, 1), 3.14159265358979, 1e-9);
}
