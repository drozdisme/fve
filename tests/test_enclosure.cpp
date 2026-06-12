#include "../core/enclosure/enclosure.hpp"
#include "../core/verifier/verifier.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

namespace {
Nodep ms_node() {
    auto num = var("sall");
    auto den = oper(Op::Mul, {var("c"), var("L")});
    return oper(Op::Sub, {oper(Op::Div, {num, den}), konst(1.0)});
}
double ms_truth(double sall, double c, double L) { return sall / (c * L) - 1.0; }
}

void test_enclosure() {
    cur = "enclosure";
    HoleTable holes;

    Program p;
    p.root = ms_node();
    p.box = {{"sall", Ival::point(300)}, {"c", Ival::point(1)}, {"L", Ival{90, 110}}};
    p.holes = holes;
    EvalResult r = eval(p);
    CHECK(r.ms.contains(ms_truth(300, 1, 100)));
    CHECK(r.ms.lo > 0.0);
    CHECK(r.trace.size() >= 5);
    CHECK(total_width(r.trace) > 0.0);

    double res = 0.0;
    Ival iv = eval_ival(ms_node(), p.box, holes, res);
    CHECK(iv == r.ms);

    Program ph;
    ph.root = oper(Op::Sub,
                   {hole("k", 1, Ival{200, 600}, 'B', {var("L")}), konst(1.0)});
    ph.box = {{"L", Ival{90, 110}}};
    EvalResult rh = eval(ph);
    CHECK(rh.residual > 0.0);
    CHECK(decide_eval(rh) == Verdict::Abstain);

    auto bundle = disj({konst(3.0), konst(8.0), konst(5.0)});
    Program pd;
    pd.root = bundle;
    EvalResult rd = eval(pd);
    CHECK(rd.ms == (Ival{3.0, 8.0}));

    std::map<std::string, Aff> env;
    env["L"] = Aff::sym(100, 10);
    env["sall"] = Aff(300);
    env["c"] = Aff(1);
    double ares = 0.0;
    Ival aiv = eval_aff(ms_node(), env, holes, ares);
    CHECK(aiv.contains(ms_truth(300, 1, 100)));

    auto mx = call("max", {var("a"), var("b")});
    auto mn = call("min", {var("a"), var("b")});
    auto ab = call("abs", {var("a")});
    Box cb = {{"a", Ival{-3, 2}}, {"b", Ival{1, 5}}};
    double cr = 0.0;
    CHECK(eval_ival(mx, cb, holes, cr) == (Ival{1, 5}));
    CHECK(eval_ival(mn, cb, holes, cr) == (Ival{-3, 2}));
    CHECK(eval_ival(ab, cb, holes, cr) == (Ival{0, 3}));
    Box cb2 = {{"a", Ival{2, 4}}};
    CHECK(eval_ival(call("abs", {var("a")}), cb2, holes, cr) == (Ival{2, 4}));
    auto unkn = call("weird", {var("a"), var("b")});
    CHECK(eval_ival(unkn, cb, holes, cr) == (Ival{-3, 5}));

    auto un = oper(Op::Sqrt, {var("a")});
    std::map<std::string, Aff> e2 = {{"a", Aff::sym(4.0, 1.0)}};
    double r2 = 0.0;
    CHECK(eval_aff(un, e2, holes, r2).contains(2.0));

    auto exn = oper(Op::Exp, {var("a")});
    CHECK(eval_aff(exn, e2, holes, r2).contains(std::exp(4.0)));

    HoleTable htab;
    htab.set_phys("z", Ival{0.0, 6.0});
    auto hn = oper(Op::Add, {hole("z", 1, Ival{0, 6}, 'A', {var("a")}), konst(1.0)});
    double r3 = 0.0;
    Ival hiv = eval_aff(hn, e2, htab, r3);
    CHECK(hiv.lo >= 0.0 && r3 > 0.0);

    auto djn = disj({konst(2.0), konst(9.0)});
    double r4 = 0.0;
    Ival djv = eval_aff(djn, e2, holes, r4);
    CHECK(djv.contains(2.0) && djv.contains(9.0) && djv.width() < 7.01);

    auto cn = call("max", {var("a"), konst(10.0)});
    double r5 = 0.0;
    CHECK(eval_aff(cn, e2, holes, r5).contains(10.0));

    auto negn = oper(Op::Neg, {var("a")});
    double r6 = 0.0;
    CHECK(eval_aff(negn, e2, holes, r6).contains(-4.0));

    Rng rng(2024);
    for (int t = 0; t < 400; t++) {
        double sall = rng.uniform(100, 500);
        double Llo = rng.uniform(50, 400), Lhi = Llo + rng.uniform(1, 100);
        Program q;
        q.root = ms_node();
        q.box = {{"sall", Ival::point(sall)}, {"c", Ival::point(1)},
                 {"L", Ival{Llo, Lhi}}};
        EvalResult qr = eval(q);
        Verdict v = decide_eval(qr);
        for (int i = 0; i < 30; i++) {
            double L = rng.uniform(Llo, Lhi);
            double truth = ms_truth(sall, 1, L);
            CHECK(qr.ms.lo - 1e-9 <= truth && truth <= qr.ms.hi + 1e-9);
            if (v == Verdict::Safe) CHECK(truth >= 0.0);
            if (v == Verdict::Fail) CHECK(truth < 1e-9);
        }
    }
}
