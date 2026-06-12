#include "../core/criticality/criticality.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

namespace {
EvalResult mk(double lo, double hi, double resid) {
    EvalResult r;
    r.ms = Ival{lo, hi};
    r.residual = resid;
    return r;
}
}

void test_criticality() {
    cur = "criticality";

    CritScore safe = criticality(mk(0.5, 0.6, 0.01));
    CHECK(safe.cs < 0.2);
    CHECK(safe.priority == 0 && safe.label == "SAFE");
    CHECK(safe.snr > 0);

    CritScore border = criticality(mk(0.0, 0.2, 0.0));
    CHECK(border.cs > 0.45 && border.cs < 0.55);
    CHECK(border.priority == 2);

    CritScore bad = criticality(mk(-1.0, -0.5, 0.0));
    CHECK(bad.cs > 0.75);
    CHECK(bad.priority >= 3);

    CritScore nan = criticality(mk(std::nan(""), 1.0, 0.0));
    CHECK(nan.cs == 1.0 && nan.priority == 4 && nan.label == "HALT");

    // monotone decreasing in ms_lo
    CHECK(criticality(mk(0.1, 0.5, 0.0)).cs < criticality(mk(0.0, 0.4, 0.0)).cs);
    // monotone increasing in width (more uncertainty -> more critical)
    CHECK(criticality(mk(0.2, 0.8, 0.0)).cs > criticality(mk(0.2, 0.3, 0.0)).cs);

    EvalResult r = mk(-0.1, 0.4, 0.0);
    r.trace.push_back({{}, "hole h1", Ival{0, 5}, 5.0, true, 'P'});
    r.trace.push_back({{}, "hole h2", Ival{0, 2}, 2.0, true, 'R'});
    r.trace.push_back({{}, "disj", Ival{0, 1}, 1.0, false, 'A'});
    r.trace.push_back({{}, "var L", Ival{90, 110}, 20.0, false, 'A'});

    AbstainReport ab = explain_abstain(r);
    CHECK(ab.is_abstain);
    CHECK(ab.reasons.size() == 3); // 2 holes + 1 disj
    // sorted by width share desc
    for (size_t i = 1; i < ab.reasons.size(); i++)
        CHECK(ab.reasons[i - 1].width_share >= ab.reasons[i].width_share);
    bool refuted = false, phys = false, disj = false;
    for (const auto& x : ab.reasons) {
        if (x.kind == "hole_refuted") refuted = true;
        if (x.kind == "hole_phys") phys = true;
        if (x.kind == "disjunction") disj = true;
    }
    CHECK(refuted && phys && disj);
    CHECK(ab.summary.find("hole") != std::string::npos);

    auto plan = reduction_plan(r);
    CHECK(plan.size() == 2);
    CHECK(plan[0].hole_id == "h1"); // widest first
    CHECK(plan[0].width_contribution > plan[1].width_contribution);
}
