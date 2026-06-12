#include "../core/verifier/verifier.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

void test_verifier() {
    cur = "verifier";
    CHECK(decide(0.1, 0.5, 0.0) == Verdict::Safe);
    CHECK(decide(-0.5, -0.1, 0.0) == Verdict::Fail);
    CHECK(decide(-0.1, 0.3, 0.0) == Verdict::Abstain);

    CHECK(decide(0.0, 0.5, 0.0) == Verdict::Abstain);
    CHECK(decide(-0.5, 0.0, 0.0) == Verdict::Abstain);

    CHECK(decide(0.3, 0.5, 0.4) == Verdict::Abstain);
    CHECK(decide(0.3, 0.5, 0.2) == Verdict::Safe);

    CHECK(decide(0.1, 0.5, -1.0) == Verdict::Abstain);
    CHECK(decide(std::nan(""), 0.5, 0.0) == Verdict::Abstain);
    double inf = 1.0 / 0.0;
    CHECK(decide(0.1, inf, 0.0) == Verdict::Abstain);

    EvalResult r;
    r.ms = Ival{0.2, 0.8};
    r.residual = 0.0;
    CHECK(decide_eval(r) == Verdict::Safe);
    r.ms = Ival::entire();
    CHECK(decide_eval(r) == Verdict::Abstain);
    r.ms = Ival::empty();
    CHECK(decide_eval(r) == Verdict::Abstain);

    CHECK(verdict_name(Verdict::Safe) == "SAFE");
    CHECK(verdict_name(Verdict::Fail) == "FAIL");
    CHECK(verdict_name(Verdict::Abstain) == "ABSTAIN");
}
