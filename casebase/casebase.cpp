#include "casebase.hpp"
#include "../core/merkle/sha256.hpp"
#include <algorithm>
#include <cmath>

namespace fve {

namespace {

JsonP box_json(const Box& b) {
    auto o = Json::mkobj();
    for (const auto& kv : b) {
        auto iv = Json::mkarr();
        iv->arr.push_back(Json::mknum(kv.second.lo));
        iv->arr.push_back(Json::mknum(kv.second.hi));
        o->obj[kv.first] = iv;
    }
    return o;
}

Box json_box(const JsonP& j) {
    Box b;
    if (!j) return b;
    for (const auto& kv : j->obj) {
        const auto& a = kv.second->arr;
        if (a.size() == 2) b[kv.first] = Ival{a[0]->num, a[1]->num};
    }
    return b;
}

std::string canon_box(const Box& b) {
    std::string s;
    for (const auto& kv : b) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s:%.9g,%.9g;", kv.first.c_str(), kv.second.lo, kv.second.hi);
        s += buf;
    }
    return s;
}

}

double case_distance(const Box& a, const Box& b) {
    double sum = 0.0;
    int n = 0;
    for (const auto& kv : a) {
        auto it = b.find(kv.first);
        if (it == b.end()) continue;
        double ma = kv.second.mid(), mb = it->second.mid();
        double denom = std::max({kv.second.width(), it->second.width(), std::fabs(ma) * 0.01, 1e-9});
        double term = (ma - mb) / denom;
        sum += term * term;
        n++;
    }
    if (n == 0) return 1e9;
    return std::sqrt(sum) / std::sqrt((double)n);
}

std::string CaseBase::make_id(const std::string& ph, const Box& box) {
    return "case_" + hex(sha256(ph + "|" + canon_box(box))).substr(0, 24);
}

std::string CaseBase::record(const Case& c) {
    std::string id = c.id.empty() ? make_id(c.program_hash, c.box) : c.id;
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(id);
    d->obj["program_hash"] = Json::mkstr(c.program_hash);
    d->obj["artifact_id"] = Json::mkstr(c.artifact_id);
    d->obj["box"] = box_json(c.box);
    auto nrm = Json::mkobj();
    for (const auto& kv : c.box) nrm->obj[kv.first] = Json::mknum(kv.second.mid());
    d->obj["box_norm"] = nrm;
    d->obj["verdict"] = Json::mkstr(verdict_name(c.verdict));
    d->obj["cs"] = Json::mknum(c.cs);
    d->obj["cert_hash"] = Json::mkstr(c.cert_hash);
    store_.put("cases", id, d);
    return id;
}

void CaseBase::record_fix(const Fix& f) {
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(f.id);
    d->obj["description"] = Json::mkstr(f.description);
    d->obj["patch"] = box_json(f.box_patch);
    d->obj["status"] = Json::mkstr("proposed");
    store_.put("fixes", f.id, d);
}

void CaseBase::approve_fix(const std::string& fix_id, const std::string& approver) {
    JsonP fj = store_.get("fixes", fix_id);
    if (!fj) return;
    fj->obj["status"] = Json::mkstr("approved");
    fj->obj["approved_by"] = Json::mkstr(approver);
    store_.put("fixes", fix_id, fj);
}

void CaseBase::record_outcome(const std::string& case_id, const std::string& fix_id,
                              const std::string& outcome, const std::string& verified_cert) {
    auto d = Json::mkobj();
    std::string id = case_id + "__" + fix_id;
    d->obj["case_id"] = Json::mkstr(case_id);
    d->obj["fix_id"] = Json::mkstr(fix_id);
    d->obj["outcome"] = Json::mkstr(outcome);
    d->obj["verified_cert"] = Json::mkstr(verified_cert);
    store_.put("case_fix_links", id, d);
}

std::vector<CaseMatch> CaseBase::query(const std::string& program_hash, const Box& box,
                                       double delta, int top_k) const {
    std::vector<CaseMatch> hits;
    for (const auto& id : store_.list("cases")) {
        JsonP c = store_.get("cases", id);
        if (!c || c->str("program_hash") != program_hash) continue;
        Box cb = json_box(c->at("box"));
        double d = case_distance(box, cb);
        if (d >= delta) continue;
        // find a resolved fix for this case
        Fix fix;
        bool has_fix = false;
        for (const auto& lid : store_.list("case_fix_links")) {
            JsonP link = store_.get("case_fix_links", lid);
            if (!link || link->str("case_id") != id || link->str("outcome") != "resolved") continue;
            JsonP fj = store_.get("fixes", link->str("fix_id"));
            if (!fj) continue;
            if (fj->str("status") != "approved") continue; // change control: only approved fixes auto-transfer
            fix.id = fj->str("id");
            fix.description = fj->str("description");
            fix.box_patch = json_box(fj->at("patch"));
            has_fix = true;
            break;
        }
        if (!has_fix) continue;
        CaseMatch m;
        m.ref.id = id;
        m.ref.program_hash = program_hash;
        m.ref.box = cb;
        m.ref.cert_hash = c->str("cert_hash");
        m.distance = d;
        m.fix = fix;
        hits.push_back(m);
    }
    std::sort(hits.begin(), hits.end(), [](const CaseMatch& a, const CaseMatch& b) { return a.distance < b.distance; });
    if ((int)hits.size() > top_k) hits.resize(top_k);
    return hits;
}

std::optional<CaseMatch> CaseBase::try_transfer(const Program& p, const CaseMatch& m) const {
    Program q = p;
    for (const auto& kv : m.fix.box_patch) q.box[kv.first] = kv.second;
    EvalResult r = eval(q);
    Certificate cert = build_cert(q, r, "case_transfer");
    if (!validate(cert)) return std::nullopt;
    if (decide_eval(r) != Verdict::Safe) return std::nullopt;
    CaseMatch out = m;
    out.transfer_verified = true;
    out.transfer_cert = cert;
    return out;
}

}
