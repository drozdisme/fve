#include "engine.hpp"
#include "ast/ast.hpp"

namespace fve {

Program load_program(const std::string& json_text, const HoleTable& holes) {
    JsonP j = parse(json_text);
    Program p;
    p.holes = holes;
    p.root = from_json(j->at("ast"));
    p.ks = j->n("ks", 0.0);
    if (j->has("box")) {
        for (const auto& kv : j->at("box")->obj) {
            auto a = kv.second;
            if (a->t == Json::Arr && a->arr.size() >= 2)
                p.box[kv.first] = Ival::of(a->arr[0]->num, a->arr[1]->num);
        }
    }
    return p;
}

RunResult run(const Program& p, AuditChain& chain, const std::string& method) {
    RunResult r;
    r.eval = eval(p);
    r.verdict = decide_eval(r.eval);
    r.cert = build_cert(p, r.eval, method);
    Digest leaf = leaf_hash(cert_dump(r.cert));
    r.audit_leaf = chain.append(leaf);
    r.report = make_report(r);
    return r;
}

JsonP make_report(const RunResult& r) {
    auto j = Json::mkobj();
    j->obj["verdict"] = Json::mkstr(verdict_name(r.verdict));
    auto enc = Json::mkobj();
    enc->obj["lower"] = Json::mknum(r.eval.ms.lo);
    enc->obj["upper"] = Json::mknum(r.eval.ms.hi);
    j->obj["ms_enclosure"] = enc;
    j->obj["residual"] = Json::mknum(r.eval.residual);
    j->obj["certificate_ref"] = Json::mkstr("sha256:" + hex(r.cert.digest));
    j->obj["audit_leaf"] = Json::mkstr("sha256:" + hex(r.audit_leaf));
    j->obj["regime"] = Json::mkstr(std::string(1, r.cert.regime));
    auto tr = Json::mkarr();
    for (const auto& t : r.eval.trace) {
        auto e = Json::mkobj();
        e->obj["node"] = Json::mkstr(hex(t.node).substr(0, 12));
        e->obj["op"] = Json::mkstr(t.label);
        auto iv = Json::mkarr();
        iv->arr.push_back(Json::mknum(t.encl.lo));
        iv->arr.push_back(Json::mknum(t.encl.hi));
        e->obj["encl"] = iv;
        e->obj["width"] = Json::mknum(t.width);
        if (t.holed) e->obj["holed"] = Json::mkbool(true);
        tr->arr.push_back(e);
    }
    j->obj["derivation"] = tr;
    return j;
}

}
