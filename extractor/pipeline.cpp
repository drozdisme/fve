#include "pipeline.hpp"
#include "xlsx/xlsx.hpp"
#include "xlsb/xlsb.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace fve {

namespace {
std::string ext_of(const std::string& p) {
    size_t d = p.rfind('.');
    std::string e = d == std::string::npos ? "" : p.substr(d + 1);
    for (char& c : e) c = std::tolower((unsigned char)c);
    return e;
}
}

Extracted extract_file(const std::string& path) {
    Extracted ex;
    std::string e = ext_of(path);
    LoadResult lr;
    if (e == "xlsb") lr = load_xlsb_file(path);
    else lr = load_xlsx_file(path);
    if (!lr.ok) { ex.err = lr.err.empty() ? "load failed" : lr.err; return ex; }
    ex.wb = lr.wb;
    ex.fmt = lr.fmt;
    Oracle orc(ex.wb);
    orc.build();
    ex.dep = induce_deps(orc);
    ex.sdg = build_sdg(ex.wb, ex.dep);
    ex.ok = true;
    return ex;
}

std::vector<CellId> margin_targets(const Sdg& sdg) {
    std::vector<CellId> outs;
    std::set<long long> is_src;
    auto key = [](const CellId& a) { return ((long long)a.sheet << 40) | ((long long)a.row << 20) | a.col; };
    for (const auto& e : sdg.edges) is_src.insert(key(e.from));
    for (const auto& n : sdg.nodes) {
        if (n.is_input) continue;
        if (is_src.count(key(n.id))) continue;
        outs.push_back(n.id);
    }
    return outs;
}

Verified verify_target(Workbook& wb, Oracle& orc, const Sdg& sdg, const CellId& target,
                       double rel_dev, AuditChain& chain) {
    Verified v;
    v.target = target;
    const SdgNode* nd = sdg.find(target);
    Ref tref{target.sheet, target.row, target.col};
    v.label = nd && !nd->label.empty() ? nd->label : wb.key(tref);
    v.lowered = lower_target(wb, orc, target);
    if (!v.lowered.ok) return v;

    Program p;
    p.root = v.lowered.root;
    for (auto& kv : v.lowered.input_base) {
        double b = kv.second;
        double d = std::fabs(b) * rel_dev;
        p.box[kv.first] = Ival::of(b - d, b + d);
    }
    v.run = run(p, chain, v.lowered.holes > 0 ? "interval+holes" : "interval");
    v.ok = true;
    return v;
}

}
