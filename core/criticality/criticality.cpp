#include "criticality.hpp"
#include "../merkle/sha256.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>

namespace fve {

std::string CritConfig::canonical() const {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", alpha);
    std::string s = "{\"alpha\":";
    s += buf;
    s += ",\"schema\":\"" + schema_version + "\",\"thresholds\":{";
    bool first = true;
    for (const auto& kv : thresholds) {
        if (!first) s += ",";
        first = false;
        std::snprintf(buf, sizeof(buf), "%.9g", kv.second);
        s += "\"" + kv.first + "\":" + buf;
    }
    s += "}}";
    return s;
}

Digest CritConfig::hash() const { return sha256(canonical()); }

namespace {
CritScore classify_cs(double cs, double snr, const std::map<std::string, double>& th) {
    auto get = [&](const std::string& k, double d) { auto it = th.find(k); return it == th.end() ? d : it->second; };
    double t1 = get("P1", 0.2), t2 = get("P2", 0.5), t3 = get("P3", 0.75), t4 = get("P4", 0.9);
    CritScore c;
    c.cs = cs;
    c.snr = snr;
    if (cs < t1) { c.priority = 0; c.label = "SAFE"; c.why = "MS reliably positive, tight enclosure"; }
    else if (cs < t2) { c.priority = 1; c.label = "WATCH"; c.why = "positive but thin safety margin"; }
    else if (cs < t3) { c.priority = 2; c.label = "REVIEW"; c.why = "enclosure straddles the safety boundary"; }
    else if (cs < t4) { c.priority = 3; c.label = "CRITICAL"; c.why = "margin likely violated or very uncertain"; }
    else { c.priority = 4; c.label = "HALT"; c.why = "MS negative or enclosure dominated by uncertainty"; }
    return c;
}
}

CritScore criticality(const EvalResult& r, const CritConfig& cfg) {
    if (std::isnan(r.ms.lo) || std::isnan(r.ms.hi) || std::isnan(r.residual)) {
        CritScore c;
        c.cs = 1.0;
        c.snr = -std::numeric_limits<double>::infinity();
        c.priority = 4;
        c.label = "HALT";
        c.why = "data corruption (NaN) - maximum caution";
        return c;
    }
    const double eps = 1e-12;
    double w = r.ms.width();
    double snr = (r.ms.lo - r.residual) / (w + r.residual + eps);
    double cs = 1.0 / (1.0 + std::exp(cfg.alpha * snr));
    return classify_cs(cs, snr, cfg.thresholds);
}

CritScore criticality(const EvalResult& r, double alpha) {
    CritScore c;
    if (std::isnan(r.ms.lo) || std::isnan(r.ms.hi) || std::isnan(r.residual)) {
        c.cs = 1.0;
        c.snr = -std::numeric_limits<double>::infinity();
        c.priority = 4;
        c.label = "HALT";
        c.why = "data corruption (NaN) — maximum caution";
        return c;
    }
    const double eps = 1e-12;
    double w = r.ms.width();
    c.snr = (r.ms.lo - r.residual) / (w + r.residual + eps);
    c.cs = 1.0 / (1.0 + std::exp(alpha * c.snr));
    if (c.cs < 0.2) { c.priority = 0; c.label = "SAFE"; c.why = "MS reliably positive, tight enclosure"; }
    else if (c.cs < 0.5) { c.priority = 1; c.label = "WATCH"; c.why = "positive but thin safety margin"; }
    else if (c.cs < 0.75) { c.priority = 2; c.label = "REVIEW"; c.why = "enclosure straddles the safety boundary"; }
    else if (c.cs < 0.9) { c.priority = 3; c.label = "CRITICAL"; c.why = "margin likely violated or very uncertain"; }
    else { c.priority = 4; c.label = "HALT"; c.why = "MS negative or enclosure dominated by uncertainty"; }
    return c;
}

namespace {

bool is_hole(const std::string& label) { return label.compare(0, 5, "hole ") == 0; }

std::string hole_id_of(const std::string& label) {
    return is_hole(label) ? label.substr(5) : label;
}

}

std::vector<CritReduction> reduction_plan(const EvalResult& r) {
    double total = total_width(r.trace);
    std::vector<CritReduction> out;
    if (total <= 0) return out;
    for (const auto& t : r.trace) {
        if (!t.holed) continue;
        CritReduction cr;
        cr.hole_id = hole_id_of(t.label);
        cr.width_contribution = t.width / total;
        cr.suggestion = "add physical bounds / input+output measurements for " + cr.hole_id;
        out.push_back(cr);
    }
    std::sort(out.begin(), out.end(),
              [](const CritReduction& a, const CritReduction& b) { return a.width_contribution > b.width_contribution; });
    return out;
}

AbstainReport explain_abstain(const EvalResult& r) {
    AbstainReport rep;
    double total = total_width(r.trace);
    if (total <= 0) total = 1.0;
    double widest_input = 0.0;
    for (const auto& t : r.trace) {
        bool disj = t.label == "disj";
        bool hole = t.holed;
        bool wide_var = t.label.compare(0, 4, "var ") == 0;
        if (wide_var) widest_input = std::max(widest_input, t.width);
        if (!hole && !disj) continue;
        AbstainReason ar;
        ar.node_id = hex(t.node).substr(0, 16);
        ar.node_label = t.label;
        ar.width_share = t.width / total;
        if (hole) {
            ar.kind = (t.regime == 'R') ? "hole_refuted" : "hole_phys";
            ar.instruction = (t.regime == 'R')
                                  ? "wrapper refuted — revisit the regularity assumption for " + hole_id_of(t.label)
                                  : "no wrapper — add measured input/output bounds for " + hole_id_of(t.label);
        } else {
            ar.kind = "disjunction";
            ar.instruction = "formula ambiguity — disambiguate the underlying cell/formula";
        }
        rep.reasons.push_back(ar);
    }
    std::sort(rep.reasons.begin(), rep.reasons.end(),
              [](const AbstainReason& a, const AbstainReason& b) { return a.width_share > b.width_share; });
    int holes = 0;
    double hole_share = 0.0;
    for (const auto& ar : rep.reasons)
        if (ar.kind.compare(0, 4, "hole") == 0) { holes++; hole_share += ar.width_share; }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%d hole(s) account for %.0f%% of uncertainty", holes, hole_share * 100.0);
    rep.summary = rep.reasons.empty() ? "no decomposable uncertainty source" : buf;
    rep.is_abstain = true;
    return rep;
}

}
