#include "proof.hpp"
#include "../ast/ast.hpp"
#include <cmath>

namespace fve {

namespace {
char rg_of(const EvalResult& r) {
    char g = 'A';
    for (const auto& t : r.trace)
        if (t.holed && t.regime == 'B') g = 'B';
    return g;
}
}

Certificate build_cert(const Program& p, const EvalResult& r,
                       const std::string& method) {
    Certificate c;
    c.program_hash = p.root->hash;
    c.ms_lo = r.ms.lo;
    c.ms_hi = r.ms.hi;
    c.residual = r.residual;
    c.ks = p.ks;
    c.method = method;
    c.regime = rg_of(r);
    c.verdict = decide_eval(r);
    for (const auto& t : r.trace)
        if (t.holed && t.regime == 'B')
            c.conditions.push_back("regime_B_class_unrefuted:" + t.label);
    if (c.verdict == Verdict::Abstain)
        c.conditions.push_back("enclosure_too_wide_or_undecidable");
    c.digest = cert_hash(c);
    return c;
}

JsonP cert_json(const Certificate& c) {
    auto j = Json::mkobj();
    j->obj["program_hash"] = Json::mkstr(hex(c.program_hash));
    j->obj["ms_lo"] = Json::mknum(c.ms_lo);
    j->obj["ms_hi"] = Json::mknum(c.ms_hi);
    j->obj["residual"] = Json::mknum(c.residual);
    j->obj["ks"] = Json::mknum(c.ks);
    j->obj["method"] = Json::mkstr(c.method);
    j->obj["regime"] = Json::mkstr(std::string(1, c.regime));
    j->obj["verdict"] = Json::mkstr(verdict_name(c.verdict));
    auto a = Json::mkarr();
    for (const auto& s : c.conditions) a->arr.push_back(Json::mkstr(s));
    j->obj["conditions"] = a;
    return j;
}

Certificate cert_from_json(const JsonP& j) {
    Certificate c;
    c.program_hash = from_hex(j->str("program_hash"));
    c.ms_lo = j->n("ms_lo");
    c.ms_hi = j->n("ms_hi");
    c.residual = j->n("residual");
    c.ks = j->n("ks");
    c.method = j->str("method");
    std::string rg = j->str("regime", "A");
    c.regime = rg.empty() ? 'A' : rg[0];
    std::string v = j->str("verdict");
    c.verdict = v == "SAFE" ? Verdict::Safe : v == "FAIL" ? Verdict::Fail : Verdict::Abstain;
    for (const auto& s : j->at("conditions")->arr) c.conditions.push_back(s->s);
    c.digest = cert_hash(c);
    return c;
}

Digest cert_hash(const Certificate& c) {
    Certificate t = c;
    t.digest = Digest{};
    return sha256(dump(cert_json(t)));
}

bool validate(const Certificate& c) {
    if (cert_hash(c) != c.digest) return false;
    if (std::isnan(c.ms_lo) || std::isnan(c.ms_hi)) return false;
    if (c.residual < 0.0) return false;
    if (c.ms_lo > c.ms_hi) return false;
    double lower = c.ms_lo - c.residual;
    double upper = c.ms_hi + c.residual;
    switch (c.verdict) {
        case Verdict::Safe: return lower > 0.0;
        case Verdict::Fail: return upper < 0.0;
        case Verdict::Abstain: return true;
    }
    return false;
}

std::string cert_dump(const Certificate& c) {
    auto j = cert_json(c);
    j->obj["digest"] = Json::mkstr(hex(c.digest));
    return dump(j);
}

}
