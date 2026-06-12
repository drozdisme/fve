#include "../core/engine.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

namespace {
const char* SAFE_JSON = R"({
  "box": {"L":[90,110],"sall":[300,300],"c":[1,1]},
  "ast": {"k":"op","op":"sub","args":[
    {"k":"op","op":"div","args":[
      {"k":"var","sym":"sall"},
      {"k":"op","op":"mul","args":[{"k":"var","sym":"c"},{"k":"var","sym":"L"}]}]},
    {"k":"const","v":1}]}
})";
}

void test_engine() {
    cur = "engine";
    AuditChain chain;
    Program p = load_program(SAFE_JSON);
    CHECK(p.box.size() == 3);
    CHECK(p.box["L"] == (Ival{90, 110}));

    RunResult r = run(p, chain);
    CHECK(r.verdict == Verdict::Safe);
    CHECK(validate(r.cert));
    CHECK(chain.verify());
    CHECK(chain.entries().size() == 1);

    CHECK(r.report->str("verdict") == "SAFE");
    CHECK(r.report->has("ms_enclosure"));
    CHECK(r.report->has("derivation"));
    CHECK(r.report->at("derivation")->arr.size() >= 5);
    CHECK(r.report->str("certificate_ref").substr(0, 7) == "sha256:");

    RunResult r2 = run(p, chain);
    CHECK(r2.cert.digest == r.cert.digest);
    CHECK(chain.entries().size() == 2);
    CHECK(chain.verify());

    Program ph;
    HoleTable tab;
    ph.root = oper(Op::Sub, {hole("h", 1, Ival{0, 10}, 'B', {var("L")}), konst(1.0)});
    ph.box = {{"L", Ival{1, 2}}};
    ph.holes = tab;
    RunResult rh = run(ph, chain);
    CHECK(rh.verdict == Verdict::Abstain);
    CHECK(rh.cert.regime == 'B');
    CHECK(!rh.cert.conditions.empty());
}
