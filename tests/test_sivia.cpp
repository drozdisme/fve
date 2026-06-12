#include "../core/sivia/sivia.hpp"
#include "../core/ast/ast.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_sivia() {
    cur = "sivia";

    // MS(x) = x - 1 over x in [0,2] -> safe region x>1, fraction ~0.5
    Program p;
    p.root = oper(Op::Sub, {var("x"), konst(1.0)});
    p.ks = 0.0;
    Box b0 = {{"x", Ival{0.0, 2.0}}};
    SiviaConfig cfg;
    cfg.epsilon = 0.01;
    SiviaResult r = sivia(p, b0, cfg);
    CHECK(!r.safe_boxes.empty());
    CHECK(r.safe_volume_fraction > 0.45 && r.safe_volume_fraction < 0.55);
    // every safe box must actually have lo > 1
    for (const auto& sb : r.safe_boxes) CHECK(sb.at("x").lo >= 1.0 - 1e-9);
    // unknown boxes are all narrow (near the boundary x=1)
    for (const auto& ub : r.unknown_boxes) CHECK(box_max_width(ub) < cfg.epsilon + 1e-9);

    // bisect splits the widest dimension
    Box b = {{"a", Ival{0, 10}}, {"b", Ival{0, 2}}};
    auto pr = bisect(b);
    CHECK(pr.first.at("a").width() < b.at("a").width());
    CHECK(pr.first.at("b").width() == b.at("b").width());

    // 2D: MS = x + y - 1 over [0,1]^2 -> safe where x+y>1, fraction ~0.5
    Program p2;
    p2.root = oper(Op::Sub, {oper(Op::Add, {var("x"), var("y")}), konst(1.0)});
    Box b2 = {{"x", Ival{0, 1}}, {"y", Ival{0, 1}}};
    SiviaConfig cfg2;
    cfg2.epsilon = 0.05;
    SiviaResult r2 = sivia(p2, b2, cfg2);
    CHECK(r2.safe_volume_fraction > 0.35 && r2.safe_volume_fraction < 0.5);
    CHECK(r2.iterations > 1);

    // soundness: a fully-safe box returns fraction ~1
    Program p3;
    p3.root = oper(Op::Sub, {var("x"), konst(1.0)});
    Box b3 = {{"x", Ival{5, 6}}};
    SiviaResult r3 = sivia(p3, b3, cfg);
    CHECK(r3.safe_volume_fraction > 0.99);
    CHECK(r3.unknown_boxes.empty());
}
