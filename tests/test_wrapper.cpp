#include "../wrapper/synth.hpp"
#include "../oracle/runtime.hpp"
#include "../extractor/xlsx/xlsx.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

void test_wrapper() {
    cur = "wrapper";
    ProbeFn lin;
    lin.domain = {Ival{0, 10}};
    lin.f = [](const std::vector<double>& x) { return 2.0 * x[0] + 1.0; };
    LocalModel m = probe_model(lin, 6);
    CHECK(m.probes.size() == 6);
    NEAR(m.lipschitz, 2.0, 1e-6);
    CHECK(m.monotone && m.sign == 1);
    CHECK(m.range.contains(1.0) && m.range.contains(21.0));

    Ival e = wrap_enclose(m, {Ival{4, 6}}, 1.25);
    CHECK(!e.is_empty() && !e.is_entire());
    CHECK(e.lo <= 9.0 && e.hi >= 13.0);

    ProbeFn dec;
    dec.domain = {Ival{0, 5}};
    dec.f = [](const std::vector<double>& x) { return -3.0 * x[0]; };
    LocalModel md = probe_model(dec, 5);
    CHECK(md.monotone && md.sign == -1);
    NEAR(md.lipschitz, 3.0, 1e-6);

    ProbeFn two;
    two.domain = {Ival{0, 2}, Ival{0, 2}};
    two.f = [](const std::vector<double>& x) { return x[0] + x[1]; };
    LocalModel m2 = probe_model(two, 3);
    CHECK(m2.probes.size() == 9);
    Ival e2 = wrap_enclose(m2, {Ival{0.5, 1.5}, Ival{0.5, 1.5}}, 1.25);
    CHECK(e2.contains(2.0));

    ProbeFn curv;
    curv.domain = {Ival{0, 4}};
    curv.f = [](const std::vector<double>& x) { return x[0] * x[0]; };
    LocalModel base = probe_model(curv, 5);
    LocalModel adapt = probe_adaptive(curv, 5, 3);
    CHECK(adapt.probes.size() == base.probes.size() + 3);
    CHECK(adapt.lipschitz >= base.lipschitz - 1e-9);
    CHECK(adapt.range.contains(0.0) && adapt.range.contains(16.0));
}

void test_runtime() {
    cur = "runtime";
    LoadResult lr = load_xlsx_file("tests/fixtures/beam.xlsx");
    Runtime rt(lr.wb, {});
    RunStats st;
    CHECK(rt.run(st));
    CHECK(st.cells > 0);
    CHECK(st.evals >= 2);
    CHECK(!st.limit_hit);
    JsonP cap = rt.capture();
    CHECK(cap->has("inputs"));
    CHECK(cap->has("outputs"));
    CHECK(cap->at("outputs")->obj.count("Beam!B4"));

    RunLimits tight;
    tight.max_cells = 1;
    Runtime rt2(lr.wb, tight);
    RunStats st2;
    CHECK(!rt2.run(st2));
    CHECK(st2.limit_hit);
}
