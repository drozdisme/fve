#include "../casebase/casebase.hpp"
#include "../core/ast/ast.hpp"
#include "../core/merkle/sha256.hpp"
#include "framework.hpp"
#include <chrono>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string cbtd() {
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_cb_" + std::to_string(getpid()) + "_" + std::to_string(ns);
}
}

void test_casebase() {
    cur = "casebase";

    // distance: identical boxes ~0, far boxes larger
    Box a = {{"x", Ival{0.5, 0.9}}};
    Box b = {{"x", Ival{0.5, 0.9}}};
    CHECK(case_distance(a, b) < 1e-9);
    Box far = {{"x", Ival{50.0, 60.0}}};
    CHECK(case_distance(a, far) > case_distance(a, b));

    Store store(cbtd());
    CaseBase cb(store);

    // MS(x) = x - 1
    Program p;
    p.root = oper(Op::Sub, {var("x"), konst(1.0)});
    std::string ph = hex(p.root->hash);

    // current failing scenario
    Box cur_box = {{"x", Ival{0.5, 0.9}}};
    p.box = cur_box;
    CHECK(decide_eval(eval(p)) != Verdict::Safe);

    // record a reference case at the same scenario + a resolved fix that moves x into safe range
    Case c;
    c.program_hash = ph;
    c.artifact_id = "art_test";
    c.box = cur_box;
    c.verdict = Verdict::Abstain;
    std::string cid = cb.record(c);
    Fix good;
    good.id = "fix_good";
    good.description = "widen L into safe band";
    good.box_patch = {{"x", Ival{1.5, 2.5}}};
    cb.record_fix(good);
    cb.approve_fix(good.id, "reviewer1");
    cb.record_outcome(cid, good.id, "resolved", "cert_ref");

    // query finds the case with its fix
    auto matches = cb.query(ph, cur_box, 0.15, 5);
    CHECK(matches.size() == 1);
    CHECK(matches[0].fix.id == "fix_good");
    CHECK(matches[0].distance < 1e-9);

    // try_transfer applies the fix, re-verifies -> SAFE
    auto transferred = cb.try_transfer(p, matches[0]);
    CHECK(transferred.has_value());
    CHECK(transferred->transfer_verified);
    CHECK(transferred->transfer_cert.verdict == Verdict::Safe);
    CHECK(validate(transferred->transfer_cert));

    // a fix that does NOT make it safe -> try_transfer returns nullopt (soundness)
    CaseMatch bad = matches[0];
    bad.fix.box_patch = {{"x", Ival{0.2, 0.4}}}; // MS=[-0.8,-0.6] still unsafe
    auto bad_t = cb.try_transfer(p, bad);
    CHECK(!bad_t.has_value());

    // different program_hash -> no match
    CHECK(cb.query("deadbeef", cur_box, 0.15, 5).empty());
    // far box -> no match within delta
    CHECK(cb.query(ph, far, 0.15, 5).empty());

    // change control: a case whose only fix is 'proposed' yields no transferable match
    Box other = {{"x", Ival{0.7, 0.95}}};
    Case c2;
    c2.program_hash = ph;
    c2.box = other;
    std::string cid2 = cb.record(c2);
    Fix prop;
    prop.id = "fix_proposed";
    prop.description = "awaiting review";
    prop.box_patch = {{"x", Ival{1.5, 2.5}}};
    cb.record_fix(prop); // stays 'proposed'
    cb.record_outcome(cid2, prop.id, "resolved", "");
    bool found_proposed = false;
    for (const auto& m : cb.query(ph, other, 0.15, 5))
        if (m.fix.id == "fix_proposed") found_proposed = true;
    CHECK(!found_proposed);
    // after approval it becomes transferable
    cb.approve_fix(prop.id, "reviewer1");
    bool found_after = false;
    for (const auto& m : cb.query(ph, other, 0.15, 5))
        if (m.fix.id == "fix_proposed") found_after = true;
    CHECK(found_after);
}
