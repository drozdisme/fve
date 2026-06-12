#include "../api/protobuf.hpp"
#include "../api/grpc.hpp"
#include "../platform/service.hpp"
#include "framework.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string gtd(const std::string& t) {
    static int c = 0;
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_grpc_" + t + "_" + std::to_string(getpid()) + "_" + std::to_string(ns) + "_" + std::to_string(c++);
}
std::vector<uint8_t> rf(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}
}

void test_grpc() {
    cur = "grpc";
    PbWriter w;
    w.varint(1, 300);
    w.dbl(2, 3.14159);
    w.str(3, "hello");
    w.boolean(4, true);
    PbWriter sub;
    sub.str(1, "nested");
    sub.varint(2, 7);
    w.message(5, sub);

    PbReader rd(w.data());
    PbField f;
    int seen = 0;
    while (rd.next(f)) {
        if (f.field == 1) { CHECK(f.v == 300); seen++; }
        else if (f.field == 2) { NEAR(PbReader::as_double(f.v), 3.14159, 1e-9); seen++; }
        else if (f.field == 3) { CHECK(std::string(f.bytes.begin(), f.bytes.end()) == "hello"); seen++; }
        else if (f.field == 4) { CHECK(f.v == 1); seen++; }
        else if (f.field == 5) {
            PbReader sr(std::string(f.bytes.begin(), f.bytes.end()));
            PbField sf;
            while (sr.next(sf)) {
                if (sf.field == 1) CHECK(std::string(sf.bytes.begin(), sf.bytes.end()) == "nested");
                if (sf.field == 2) CHECK(sf.v == 7);
            }
            seen++;
        }
    }
    CHECK(seen == 5);

    std::string framed = grpc_response("PAYLOAD", 0);
    CHECK((uint8_t)framed[0] == 0x00);
    uint32_t len = ((uint8_t)framed[1] << 24) | ((uint8_t)framed[2] << 16) | ((uint8_t)framed[3] << 8) | (uint8_t)framed[4];
    CHECK(len == 7);
    CHECK(framed.substr(5, 7) == "PAYLOAD");
    CHECK((uint8_t)framed[12] == 0x80);
    CHECK(framed.find("grpc-status:0") != std::string::npos);

    std::string body;
    body += (char)0x00;
    std::string payload = "abc";
    body += (char)0;
    body += (char)0;
    body += (char)0;
    body += (char)payload.size();
    body += payload;
    std::string msg;
    CHECK(grpc_unframe(body, msg));
    CHECK(msg == "abc");
    std::string tooshort = "xy";
    CHECK(!grpc_unframe(tooshort, msg));

    Service svc(gtd("svc"));
    Grpc g(svc);

    PbWriter up;
    up.str(1, "beam.xlsx");
    up.str(2, "application/x");
    auto bytes = rf("tests/fixtures/beam.xlsx");
    up.bytes(3, bytes);
    int st = -1;
    std::string upr = g.dispatch("Upload", up.data(), st);
    CHECK(st == 0);
    std::string aid;
    {
        PbReader r(upr);
        PbField ff;
        while (r.next(ff)) if (ff.field == 1) aid = std::string(ff.bytes.begin(), ff.bytes.end());
    }
    CHECK(aid.substr(0, 4) == "art_");

    PbWriter an;
    an.str(1, aid);
    std::string anr = g.dispatch("Analyze", an.data(), st);
    CHECK(st == 0);
    bool an_ok = false;
    {
        PbReader r(anr);
        PbField ff;
        while (r.next(ff)) if (ff.field == 1) an_ok = ff.v == 1;
    }
    CHECK(an_ok);

    PbWriter vr;
    vr.str(1, aid);
    vr.dbl(2, 0.05);
    std::string vrr = g.dispatch("Verify", vr.data(), st);
    CHECK(st == 0);
    bool v_ok = false;
    int targets = 0, safe = 0;
    {
        PbReader r(vrr);
        PbField ff;
        while (r.next(ff)) {
            if (ff.field == 1) v_ok = ff.v == 1;
            else if (ff.field == 3) {
                targets++;
                PbReader tr(std::string(ff.bytes.begin(), ff.bytes.end()));
                PbField tf;
                while (tr.next(tf))
                    if (tf.field == 3 && std::string(tf.bytes.begin(), tf.bytes.end()) == "SAFE") safe++;
            }
        }
    }
    CHECK(v_ok);
    CHECK(targets == 2);
    CHECK(safe == 2);

    std::string run_id;
    {
        PbReader r(vrr);
        PbField ff;
        while (r.next(ff)) if (ff.field == 2) run_id = std::string(ff.bytes.begin(), ff.bytes.end());
    }
    CHECK(!run_id.empty());

    PbWriter cr;
    cr.str(1, run_id);
    std::string crr = g.dispatch("GetCertificate", cr.data(), st);
    CHECK(st == 0);
    int ct_targets = 0;
    {
        PbReader r(crr);
        PbField ff;
        while (r.next(ff)) if (ff.field == 4) ct_targets++;
    }
    CHECK(ct_targets == 2);

    PbWriter ar;
    ar.str(1, aid);
    std::string arr2 = g.dispatch("GetArtifact", ar.data(), st);
    bool has_name = false;
    {
        PbReader r(arr2);
        PbField ff;
        while (r.next(ff)) if (ff.field == 2 && std::string(ff.bytes.begin(), ff.bytes.end()) == "beam.xlsx") has_name = true;
    }
    CHECK(has_name);

    PbWriter gr;
    gr.str(1, aid);
    std::string grr = g.dispatch("GetGraph", gr.data(), st);
    int gnodes = 0;
    {
        PbReader r(grr);
        PbField ff;
        while (r.next(ff)) if (ff.field == 1) gnodes++;
    }
    CHECK(gnodes >= 5);

    PbWriter pr;
    pr.str(1, aid);
    std::string prr = g.dispatch("Provenance", pr.data(), st);
    int plinks = 0;
    {
        PbReader r(prr);
        PbField ff;
        while (r.next(ff)) if (ff.field == 1) plinks++;
    }
    CHECK(plinks >= 1);

    PbWriter au;
    std::string aur = g.dispatch("AuditHistory", au.data(), st);
    bool chain_ok = false;
    int entries = 0;
    {
        PbReader r(aur);
        PbField ff;
        while (r.next(ff)) {
            if (ff.field == 1) chain_ok = ff.v == 1;
            else if (ff.field == 2) entries++;
        }
    }
    CHECK(chain_ok);
    CHECK(entries >= 3);

    int us = 0;
    g.dispatch("DoesNotExist", "", us);
    CHECK(us == 12);
}
