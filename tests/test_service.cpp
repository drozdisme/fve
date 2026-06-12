#include "../platform/service.hpp"
#include "../api/http.hpp"
#include "framework.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <thread>

using namespace fve;
using namespace fvetest;

namespace {
std::vector<uint8_t> read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}
std::string td(const std::string& t) {
    static int ctr = 0;
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_svc_" + t + "_" + std::to_string(getpid()) + "_" +
           std::to_string(ns) + "_" + std::to_string(ctr++);
}
}

void test_service() {
    cur = "service";
    Service svc(td("beam"));
    auto bytes = read_file("tests/fixtures/beam.xlsx");
    CHECK(!bytes.empty());
    std::string aid = svc.ingest("beam.xlsx", "application/x", bytes);
    CHECK(aid.substr(0, 4) == "art_");

    JsonP an = svc.analyze(aid);
    CHECK(an != nullptr);
    CHECK(an->at("ok")->b);
    CHECK(an->at("analysis")->at("glue_consistent")->b);

    JsonP v = svc.verify(aid, 0.05);
    CHECK(v->at("ok")->b);
    auto& targets = v->at("targets")->arr;
    CHECK(targets.size() == 2);
    int safe = 0;
    for (auto& t : targets) {
        if (t->str("verdict") == "SAFE") safe++;
        CHECK(t->at("cert_valid")->b);
    }
    CHECK(safe == 2);

    std::string rid = v->str("run_id");
    JsonP cert = svc.get_certificate(rid);
    CHECK(cert && cert->str("artifact") == aid);

    JsonP g = svc.get_graph(aid);
    CHECK(g->at("ok")->b);
    CHECK(g->at("nodes")->arr.size() >= 5);

    CHECK(svc.audit_ok());
    CHECK(!svc.audit_history()->arr.empty());
    CHECK(!svc.provenance(aid)->arr.empty());

    Service svc2(td("holed"));
    auto hb = read_file("tests/fixtures/holed.xlsx");
    std::string hid = svc2.ingest("holed.xlsx", "application/x", hb);
    svc2.analyze(hid);
    JsonP hv = svc2.verify(hid, 0.03);
    CHECK(hv->at("ok")->b);
    bool wrapper_used = false;
    for (auto& t : hv->at("targets")->arr)
        if (t->str("method") == "wrapper_synthesis") wrapper_used = true;
    CHECK(wrapper_used);

    Service svc3(td("img"));
    auto ib = read_file("tests/fixtures/eq1.png");
    std::string iid = svc3.ingest("eq1.png", "image/png", ib);
    JsonP rec = svc3.recognize(iid);
    CHECK(rec->at("ok")->b);
    CHECK(rec->str("formula") == "a/(b*c)-1");
    CHECK(!rec->at("ambiguous")->b);
}

void test_http() {
    cur = "http";
    CHECK(url_decode("a%20b") == "a b");
    CHECK(url_decode("x+y") == "x y");
    CHECK(url_decode("100%25") == "100%");

    std::string ct = "multipart/form-data; boundary=----X";
    std::string body =
        "------X\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"a.xlsx\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n"
        "HELLOBYTES\r\n"
        "------X--\r\n";
    std::string fn;
    std::string data = multipart_file(body, ct, fn);
    CHECK(fn == "a.xlsx");
    CHECK(data == "HELLOBYTES");

    Req req;
    req.headers["content-type"] = "text/plain";
    CHECK(req.header("Content-Type") == "text/plain");
    CHECK(req.header("X-Missing") == "");

    Res r = Res::json("{\"a\":1}");
    CHECK(r.status == 200 && r.ctype == "application/json");
    Res t = Res::text("hi", 404);
    CHECK(t.status == 404);

    Http h;
    h.route("GET", "/api/items/:id", [](const Req& q) {
        return Res::json("{\"id\":\"" + q.params.at("id") + "\"}");
    });
    h.route("POST", "/api/items", [](const Req&) { return Res::json("{\"made\":true}"); });

    Req q1;
    q1.method = "GET";
    q1.path = "/api/items/abc";
    Res rr = h.handle(q1);
    CHECK(rr.status == 200);
    CHECK(rr.body.find("abc") != std::string::npos);

    Req q2;
    q2.method = "POST";
    q2.path = "/api/items";
    CHECK(h.handle(q2).body.find("made") != std::string::npos);

    Req q3;
    q3.method = "GET";
    q3.path = "/api/unknown/route";
    CHECK(h.handle(q3).status == 404);

    Req q4;
    q4.method = "DELETE";
    q4.path = "/api/items/x";
    CHECK(h.handle(q4).status == 404);

    Http hs;
    hs.static_dir("/", "ui");
    Req q5;
    q5.method = "GET";
    q5.path = "/style.css";
    Res sr = hs.handle(q5);
    CHECK(sr.status == 200 && sr.ctype == "text/css");
    Req q6;
    q6.method = "GET";
    q6.path = "/";
    CHECK(hs.handle(q6).ctype == "text/html");

    Http hc;
    hc.route("GET", "/ping", [](const Req&) { return Res::json("{\"p\":1}"); });
    hc.route("POST", "/echo", [](const Req& q) { return Res::text(q.body); });
    int sv[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    std::string reqstr = "GET /ping?x=1 HTTP/1.1\r\nHost: t\r\nContent-Length: 0\r\n\r\n";
    ssize_t wn = ::write(sv[0], reqstr.data(), reqstr.size());
    CHECK(wn == (ssize_t)reqstr.size());
    std::thread th([&] { hc.serve_conn(sv[1]); });
    std::string resp;
    char rb[2048];
    ssize_t rn;
    while ((rn = ::read(sv[0], rb, sizeof(rb))) > 0) resp.append(rb, rn);
    th.join();
    ::close(sv[0]);
    CHECK(resp.find("200") != std::string::npos);
    CHECK(resp.find("{\"p\":1}") != std::string::npos);

    int sv2[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv2);
    std::string post = "POST /echo HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello";
    ssize_t wn2 = ::write(sv2[0], post.data(), post.size());
    CHECK(wn2 == (ssize_t)post.size());
    std::thread th2([&] { hc.serve_conn(sv2[1]); });
    std::string resp2;
    while ((rn = ::read(sv2[0], rb, sizeof(rb))) > 0) resp2.append(rb, rn);
    th2.join();
    ::close(sv2[0]);
    CHECK(resp2.find("hello") != std::string::npos);

    std::string fn2;
    std::string mp =
        "------B\r\nContent-Disposition: form-data; name=\"f\"; filename=\"d.bin\"\r\n\r\n"
        "DATA\r\n------B--\r\n";
    std::string got = multipart_file(mp, "multipart/form-data; boundary=----B", fn2);
    CHECK(fn2 == "d.bin" && got == "DATA");
    std::string nofn;
    CHECK(multipart_file("nope", "text/plain", nofn).empty());
}
