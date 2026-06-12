#include "../semantic/glue.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_semantic() {
    cur = "semantic";
    Section a;
    a.region = "A";
    a.sym["L"] = {Ival{90, 110}, Dim::base(1), true};
    a.sym["F"] = {Ival{0, 500}, Dim::base(0), true};
    Section b;
    b.region = "B";
    b.sym["L"] = {Ival{95, 120}, Dim::base(1), true};
    b.sym["sigma"] = {Ival{0, 300}, Dim::none(), true};

    auto m = match_sections(a, b);
    CHECK(m.size() == 1);

    GlueResult g = glue({a, b});
    CHECK(g.consistent);
    CHECK(!g.obstructed);
    CHECK(g.global.sym.count("L"));
    CHECK(g.global.sym["L"].range == (Ival{95, 110}));
    CHECK(g.global.sym.count("F") && g.global.sym.count("sigma"));

    Section c;
    c.region = "C";
    c.sym["L"] = {Ival{200, 300}, Dim::base(1), true};
    GlueResult g2 = glue({a, c});
    CHECK(!g2.consistent);
    CHECK(g2.obstructed);
    bool range_clash = false;
    for (auto& cf : g2.conflicts) if (cf.symbol == "L" && cf.kind == "range") range_clash = true;
    CHECK(range_clash);

    Section d;
    d.region = "D";
    d.sym["L"] = {Ival::entire(), Dim::base(2), true};
    GlueResult g3 = glue({a, d});
    CHECK(!g3.consistent);
    bool dim_clash = false;
    for (auto& cf : g3.conflicts) if (cf.kind == "dimension") dim_clash = true;
    CHECK(dim_clash);

    Section e1, e2, e3;
    e1.region = "1"; e1.sym["x"] = {Ival{0, 10}, Dim::none(), false};
    e2.region = "2"; e2.sym["x"] = {Ival{5, 15}, Dim::none(), false};
    e3.region = "3"; e3.sym["x"] = {Ival{8, 20}, Dim::none(), false};
    GlueResult g4 = glue({e1, e2, e3});
    CHECK(g4.consistent);
    CHECK(g4.global.sym["x"].range == (Ival{8, 10}));

    std::vector<std::string> regs = {"A", "B", "C"};
    Cohomology coh = cohomology(regs, {{"A", "B", "d", 1.0}, {"B", "C", "d", 1.0}, {"C", "A", "d", 1.0}});
    CHECK(coh.obstructed);
    CHECK(coh.h1_rank == 1);
    CHECK(!coh.classes.empty());
    NEAR(coh.classes[0].discrepancy, 3.0, 1e-9);

    Cohomology cob = cohomology(regs, {{"A", "B", "d", 1.0}, {"B", "C", "d", 2.0}, {"A", "C", "d", 3.0}});
    CHECK(!cob.obstructed);
    CHECK(cob.h1_rank == 1);
    CHECK(cob.classes.empty());

    Cohomology tree = cohomology(regs, {{"A", "B", "d", 1.0}, {"B", "C", "d", 2.0}});
    CHECK(!tree.obstructed);
    CHECK(tree.h1_rank == 0);

    Cohomology multi = cohomology(regs, {{"A", "B", "z", 1.0}, {"B", "A", "z", 5.0}});
    CHECK(multi.obstructed);
}
