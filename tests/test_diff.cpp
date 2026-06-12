#include "../audit/diff.hpp"
#include "../db/store.hpp"
#include "framework.hpp"
#include <chrono>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string dtd() {
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_diff_" + std::to_string(getpid()) + "_" + std::to_string(ns);
}
JsonP target(const std::string& cell, const std::string& verdict, double lo, double hi, double cs, int holes) {
    auto t = Json::mkobj();
    t->obj["cell"] = Json::mkstr(cell);
    t->obj["label"] = Json::mkstr(cell);
    t->obj["verdict"] = Json::mkstr(verdict);
    auto e = Json::mkarr();
    e->arr.push_back(Json::mknum(lo));
    e->arr.push_back(Json::mknum(hi));
    t->obj["ms_enclosure"] = e;
    t->obj["cs"] = Json::mknum(cs);
    t->obj["holes"] = Json::mknum(holes);
    return t;
}
JsonP run(const std::string& id, const std::string& art, std::vector<JsonP> ts) {
    auto r = Json::mkobj();
    r->obj["id"] = Json::mkstr(id);
    r->obj["artifact"] = Json::mkstr(art);
    auto a = Json::mkarr();
    for (auto& t : ts) a->arr.push_back(t);
    r->obj["targets"] = a;
    return r;
}
}

void test_diff() {
    cur = "diff";
    Store s(dtd());
    s.put("runs", "run_old", run("run_old", "artX",
                                  {target("S!A1", "SAFE", 0.1, 0.5, 0.18, 1),
                                   target("S!A2", "SAFE", 0.3, 0.6, 0.10, 0)}));
    s.put("runs", "run_new", run("run_new", "artX",
                                  {target("S!A1", "SAFE", 0.2, 0.5, 0.10, 0),   // improved, hole closed
                                   target("S!A2", "ABSTAIN", -0.1, 0.6, 0.62, 1)})); // regressed, hole opened

    CertDiff d = diff_runs(s, "run_old", "run_new");
    CHECK(d.ok);
    CHECK(d.artifact == "artX");
    CHECK(!d.safe); // S!A2 went SAFE->ABSTAIN
    CHECK(d.targets.size() == 2);
    // hottest first (largest |delta_cs|): A2 delta +0.52
    CHECK(d.targets[0].cell == "S!A2");
    CHECK(d.targets[0].transition == VerdictTransition::SafeAbstain);
    // A1 improved
    bool a1ok = false;
    for (const auto& t : d.targets)
        if (t.cell == "S!A1") { a1ok = (t.delta_cs < -0.05 && t.transition == VerdictTransition::SafeSafe); }
    CHECK(a1ok);
    // hole bookkeeping
    bool closed_a1 = false, new_a2 = false;
    for (const auto& c : d.closed_holes) if (c == "S!A1") closed_a1 = true;
    for (const auto& c : d.new_holes) if (c == "S!A2") new_a2 = true;
    CHECK(closed_a1 && new_a2);

    // identical runs -> safe, no regression
    CertDiff d2 = diff_runs(s, "run_old", "run_old");
    CHECK(d2.safe);

    JsonP j = cert_diff_json(d);
    CHECK(j->at("safe")->b == false);
    CHECK(j->at("targets")->arr.size() == 2);
}
