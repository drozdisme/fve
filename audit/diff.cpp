#include "diff.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace fve {

std::string transition_name(VerdictTransition t) {
    switch (t) {
        case VerdictTransition::SafeSafe: return "SAFE->SAFE";
        case VerdictTransition::SafeAbstain: return "SAFE->ABSTAIN";
        case VerdictTransition::AbstainSafe: return "ABSTAIN->SAFE";
        case VerdictTransition::ToFail: return "->FAIL";
        case VerdictTransition::FromFail: return "FAIL->";
        default: return "other";
    }
}

namespace {

Ival enc_of(const JsonP& t) {
    if (t && t->has("ms_enclosure") && t->at("ms_enclosure")->arr.size() == 2)
        return Ival{t->at("ms_enclosure")->arr[0]->num, t->at("ms_enclosure")->arr[1]->num};
    return Ival{0, 0};
}

VerdictTransition transition_of(const std::string& a, const std::string& b) {
    if (b == "FAIL" && a != "FAIL") return VerdictTransition::ToFail;
    if (a == "FAIL" && b != "FAIL") return VerdictTransition::FromFail;
    if (a == "SAFE" && b == "SAFE") return VerdictTransition::SafeSafe;
    if (a == "SAFE" && b == "ABSTAIN") return VerdictTransition::SafeAbstain;
    if (a == "ABSTAIN" && b == "SAFE") return VerdictTransition::AbstainSafe;
    return VerdictTransition::Other;
}

std::map<std::string, JsonP> targets_by_cell(const JsonP& run) {
    std::map<std::string, JsonP> m;
    if (run && run->has("targets"))
        for (const auto& t : run->at("targets")->arr) m[t->str("cell")] = t;
    return m;
}

}

CertDiff diff_runs(Store& store, const std::string& run_a, const std::string& run_b) {
    CertDiff d;
    d.run_old = run_a;
    d.run_new = run_b;
    JsonP ra = store.get("runs", run_a);
    JsonP rb = store.get("runs", run_b);
    if (!ra || !rb) return d;
    d.artifact = rb->str("artifact");
    auto ma = targets_by_cell(ra);
    auto mb = targets_by_cell(rb);

    for (const auto& kv : mb) {
        const std::string& cell = kv.first;
        JsonP tn = kv.second;
        auto it = ma.find(cell);
        TargetDelta td;
        td.cell = cell;
        td.label = tn->str("label");
        td.ms_new = enc_of(tn);
        td.cs_new = tn->n("cs");
        td.verdict_new = tn->str("verdict");
        td.holes_new = (int)tn->n("holes");
        if (it == ma.end()) {
            td.verdict_old = "(absent)";
            td.transition = VerdictTransition::Other;
            d.targets.push_back(td);
            continue;
        }
        JsonP to = it->second;
        td.ms_old = enc_of(to);
        td.cs_old = to->n("cs");
        td.verdict_old = to->str("verdict");
        td.holes_old = (int)to->n("holes");
        td.delta_lo = td.ms_new.lo - td.ms_old.lo;
        td.delta_hi = td.ms_new.hi - td.ms_old.hi;
        td.delta_width = td.ms_new.width() - td.ms_old.width();
        td.delta_cs = td.cs_new - td.cs_old;
        td.transition = transition_of(td.verdict_old, td.verdict_new);
        if (td.holes_new > td.holes_old) d.new_holes.push_back(cell);
        if (td.holes_new < td.holes_old) d.closed_holes.push_back(cell);
        if (td.transition == VerdictTransition::ToFail ||
            td.transition == VerdictTransition::SafeAbstain ||
            (td.transition == VerdictTransition::SafeSafe && td.delta_cs > 0.05))
            d.safe = false;
        d.targets.push_back(td);
    }
    std::sort(d.targets.begin(), d.targets.end(),
              [](const TargetDelta& a, const TargetDelta& b) { return std::fabs(a.delta_cs) > std::fabs(b.delta_cs); });

    int worse = 0, better = 0;
    for (const auto& t : d.targets) {
        if (t.delta_cs > 0.05) worse++;
        else if (t.delta_cs < -0.05) better++;
    }
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%zu targets: %d improved, %d regressed; %s",
                  d.targets.size(), better, worse, d.safe ? "Rev_new not worse" : "REGRESSION");
    d.summary = buf;
    d.ok = true;
    return d;
}

JsonP cert_diff_json(const CertDiff& d) {
    auto o = Json::mkobj();
    o->obj["ok"] = Json::mkbool(d.ok);
    o->obj["run_old"] = Json::mkstr(d.run_old);
    o->obj["run_new"] = Json::mkstr(d.run_new);
    o->obj["artifact"] = Json::mkstr(d.artifact);
    o->obj["safe"] = Json::mkbool(d.safe);
    o->obj["summary"] = Json::mkstr(d.summary);
    auto arr = Json::mkarr();
    for (const auto& t : d.targets) {
        auto x = Json::mkobj();
        x->obj["cell"] = Json::mkstr(t.cell);
        x->obj["label"] = Json::mkstr(t.label);
        x->obj["verdict_old"] = Json::mkstr(t.verdict_old);
        x->obj["verdict_new"] = Json::mkstr(t.verdict_new);
        x->obj["transition"] = Json::mkstr(transition_name(t.transition));
        x->obj["delta_cs"] = Json::mknum(t.delta_cs);
        x->obj["delta_width"] = Json::mknum(t.delta_width);
        x->obj["cs_old"] = Json::mknum(t.cs_old);
        x->obj["cs_new"] = Json::mknum(t.cs_new);
        arr->arr.push_back(x);
    }
    o->obj["targets"] = arr;
    auto nh = Json::mkarr();
    for (const auto& c : d.new_holes) nh->arr.push_back(Json::mkstr(c));
    auto ch = Json::mkarr();
    for (const auto& c : d.closed_holes) ch->arr.push_back(Json::mkstr(c));
    o->obj["new_holes"] = nh;
    o->obj["closed_holes"] = ch;
    return o;
}

}
