#include "../db/store.hpp"
#include "../registry/registry.hpp"
#include "../audit/audit.hpp"
#include "framework.hpp"
#include <chrono>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string tmpdir(const std::string& tag) {
    static int ctr = 0;
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_t_" + tag + "_" + std::to_string(getpid()) + "_" +
           std::to_string(ns) + "_" + std::to_string(ctr++);
}
}

void test_store() {
    cur = "store";
    Store s(tmpdir("store"));
    auto d = Json::mkobj();
    d->obj["x"] = Json::mknum(42);
    CHECK(s.put("things", "a1", d));
    CHECK(s.exists("things", "a1"));
    JsonP g = s.get("things", "a1");
    CHECK(g && (int)g->n("x") == 42);
    CHECK(!s.exists("things", "nope"));
    CHECK(s.get("things", "nope") == nullptr);
    CHECK(s.list("things").size() == 1);

    auto e1 = Json::mkobj();
    e1->obj["v"] = Json::mknum(1);
    s.append("log", e1);
    s.append("log", e1);
    CHECK(s.read_log("log").size() == 2);
}

void test_registry() {
    cur = "registry";
    Store s(tmpdir("reg"));
    Registry r(s);
    std::vector<uint8_t> bytes = {1, 2, 3, 4, 5};
    std::string aid = r.put_artifact("f.xlsx", "application/x", bytes, nullptr);
    CHECK(aid.substr(0, 4) == "art_");
    JsonP a = r.get("artifacts", aid);
    CHECK(a && a->str("name") == "f.xlsx");
    CHECK((int)a->n("size") == 5);
    CHECK(a->str("hash").substr(0, 7) == "sha256:");

    std::vector<uint8_t> same = {1, 2, 3, 4, 5};
    std::string aid2 = r.put_artifact("f.xlsx", "application/x", same, nullptr);
    CHECK(aid2 == aid);

    auto model = Json::mkobj();
    model->obj["sheets"] = Json::mknum(2);
    std::string mid = r.put_model(aid, model, nullptr);
    CHECK(mid.substr(0, 4) == "mdl_");
    CHECK(r.list("models").size() == 1);

    auto prov = r.provenance(aid);
    CHECK(!prov.empty());
    bool found = false;
    for (auto& p : prov) if (p->str("to") == mid && p->str("rel") == "produced") found = true;
    CHECK(found);
}

void test_audit() {
    cur = "audit";
    std::string dir = tmpdir("audit");
    {
        Store s(dir);
        Audit a(s);
        auto p = Json::mkobj();
        p->obj["k"] = Json::mkstr("v");
        a.record("ingest", "art_1", p);
        a.record("verify", "art_1", p);
        a.record("certificate", "MS", p);
        CHECK(a.verify());
        CHECK(a.history().size() == 3);
        CHECK(a.history("art_1").size() == 2);
        CHECK(a.certificates().size() == 1);
    }
    {
        Store s(dir);
        Audit a2(s);
        CHECK(a2.verify());
        CHECK(a2.history().size() == 3);
    }
}
