#include "coverage.hpp"
#include "../extractor/formula/formula.hpp"
#include <algorithm>
#include <cctype>

namespace fve {

std::string hole_reason_name(HoleReason r) {
    switch (r) {
        case HoleReason::ParseError: return "parse_error";
        case HoleReason::ExternalRef: return "external_ref";
        case HoleReason::UnsupportedFunction: return "unsupported_function";
        case HoleReason::EmptyOrText: return "empty_or_text";
        default: return "unknown";
    }
}

namespace {

const std::vector<std::string>& hard_functions() {
    // Reference / dynamic-array / lambda families that are not interval-liftable.
    static const std::vector<std::string> f = {
        "XLOOKUP", "VLOOKUP", "HLOOKUP", "LOOKUP", "INDEX", "MATCH", "XMATCH",
        "OFFSET", "INDIRECT", "LAMBDA", "LET", "FILTER", "UNIQUE", "SEQUENCE",
        "SORT", "SORTBY", "TEXTJOIN", "CHOOSE", "GETPIVOTDATA"};
    return f;
}

std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

}

CoverageLedger compute_coverage(const Workbook& wb) {
    CoverageLedger cl;
    for (size_t si = 0; si < wb.sheets.size(); si++) {
        const Sheet& sh = wb.sheets[si];
        for (const auto& kv : sh.cells) {
            const Cell& c = kv.second;
            bool has_formula = !c.formula.empty();
            bool has_const = c.has_value && c.formula.empty();
            bool has_text = !c.text.empty() && !c.has_value && c.formula.empty();
            if (!has_formula && !has_const && !has_text) continue;
            cl.total_cells++;
            if (has_formula) cl.formula_cells++;
            else if (has_const) cl.constant_cells++;

            if (!has_formula) continue;
            std::string up = upper(c.formula);
            std::vector<std::string> reasons;
            if (c.formula.find('[') != std::string::npos && c.formula.find(']') != std::string::npos)
                reasons.push_back("external_ref");
            for (const auto& fn : hard_functions())
                if (up.find(fn + "(") != std::string::npos)
                    reasons.push_back("unsupported_function:" + fn);
            XParse pr = parse_formula(c.formula);
            if (!pr.ok) reasons.push_back("parse_error");
            if (reasons.empty()) continue;
            cl.hole_cells++;
            for (const auto& rsn : reasons) {
                cl.hole_reasons[rsn]++;
                if (cl.samples.size() < 50)
                    cl.samples.push_back({wb.key((int)si, kv.first.first, kv.first.second), c.formula, rsn});
            }
        }
    }
    double ratio = cl.formula_cells > 0 ? (double)cl.hole_cells / cl.formula_cells : 0.0;
    if (ratio < 0.02) cl.confidence = "high";
    else if (ratio < 0.10) cl.confidence = "medium";
    else { cl.confidence = "low"; cl.flags.push_back("high_uncovered_ratio"); }
    if (cl.hole_reasons.count("external_ref")) cl.flags.push_back("external_refs_present");
    return cl;
}

void persist_coverage(Store& store, const std::string& file_id, const CoverageLedger& cl) {
    auto d = Json::mkobj();
    d->obj["file_id"] = Json::mkstr(file_id);
    d->obj["total_cells"] = Json::mknum(cl.total_cells);
    d->obj["formula_cells"] = Json::mknum(cl.formula_cells);
    d->obj["constant_cells"] = Json::mknum(cl.constant_cells);
    d->obj["hole_cells"] = Json::mknum(cl.hole_cells);
    auto hr = Json::mkobj();
    for (const auto& kv : cl.hole_reasons) hr->obj[kv.first] = Json::mknum(kv.second);
    d->obj["hole_reasons"] = hr;
    d->obj["confidence"] = Json::mkstr(cl.confidence);
    auto fl = Json::mkarr();
    for (const auto& f : cl.flags) fl->arr.push_back(Json::mkstr(f));
    d->obj["flags"] = fl;
    auto sm = Json::mkarr();
    for (const auto& s : cl.samples) {
        auto so = Json::mkobj();
        so->obj["file_id"] = Json::mkstr(file_id);
        so->obj["cell_ref"] = Json::mkstr(s.cell_ref);
        so->obj["formula"] = Json::mkstr(s.formula);
        so->obj["reason"] = Json::mkstr(s.reason);
        sm->arr.push_back(so);
    }
    d->obj["samples"] = sm;
    store.put("coverage_ledger", file_id, d);

    for (const auto& kv : cl.hole_reasons) {
        std::string cid = kv.first;
        for (char& ch : cid) if (ch == '/' || ch == '\\' || ch == ' ' || ch == ':') ch = '_';
        JsonP cur = store.get("unsupported_constructs", cid);
        int files = cur ? (int)cur->n("file_count") : 0;
        int cells = cur ? (int)cur->n("cell_count") : 0;
        auto u = Json::mkobj();
        u->obj["construct"] = Json::mkstr(kv.first);
        u->obj["file_count"] = Json::mknum(files + 1);
        u->obj["cell_count"] = Json::mknum(cells + kv.second);
        store.put("unsupported_constructs", cid, u);
    }
}

}
