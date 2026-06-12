#include "../core/holes/holes.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

void test_holes() {
    cur = "holes";
    WrapResult pw = phys_wrap(Ival{0.0, 4.0}, {});
    CHECK(pw.encl == (Ival{0.0, 4.0}));
    NEAR(pw.residual, 2.0, 1e-12);

    auto mono = monotone_wrapper(+1, Ival{0.0, 1e9},
                                 [](const std::vector<double>& x) { return 3.0 * x[0]; });
    WrapResult mr = mono({Ival{2.0, 5.0}});
    CHECK(mr.encl.contains(6.0) && mr.encl.contains(15.0));
    NEAR(mr.encl.lo, 6.0, 1e-9);
    NEAR(mr.encl.hi, 15.0, 1e-9);

    auto monodown = monotone_wrapper(-1, Ival::entire(),
                                     [](const std::vector<double>& x) { return -2.0 * x[0]; });
    WrapResult md = monodown({Ival{1.0, 3.0}});
    CHECK(md.encl.contains(-2.0) && md.encl.contains(-6.0));

    std::vector<Probe> probes = {{{0.0}, 0.0}, {{1.0}, 1.0}, {{2.0}, 2.0}, {{3.0}, 3.0}};
    auto lip = lipschitz_wrapper(probes, 1.5, Ival{-100.0, 100.0});
    WrapResult lr = lip({Ival{1.0, 1.0}});
    CHECK(lr.encl.contains(1.0));
    CHECK(!lr.refuted);

    std::vector<Probe> jumpy = {{{0.0}, 0.0}, {{1.0}, 100.0}};
    auto lip2 = lipschitz_wrapper(jumpy, 2.0, Ival{-1.0, 1.0});
    WrapResult lr2 = lip2({Ival{0.5, 0.5}});
    CHECK(lr2.encl == (Ival{-1.0, 1.0}));

    HoleTable tab;
    tab.set("k", mono);
    tab.set_phys("k", Ival{0.0, 1e9});
    CHECK(tab.has("k"));
    WrapResult te = tab.eval("k", Ival::entire(), {Ival{2.0, 5.0}});
    CHECK(te.encl.contains(6.0));
    WrapResult miss = tab.eval("none", Ival{1.0, 9.0}, {});
    CHECK(miss.encl == (Ival{1.0, 9.0}));

    Ival bh = bundle_hull({Ival{1.0, 3.0}, Ival{2.0, 8.0}, Ival{-1.0, 0.0}});
    CHECK(bh == (Ival{-1.0, 8.0}));
    CHECK(bundle_hull({}).is_empty());
}
