#include "../extractor/pipeline.hpp"
#include "../core/ast/json.hpp"
#include <cstdio>
#include <iostream>

using namespace fve;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: extract <file.xlsx|xlsm|xlsb> [rel_dev]\n";
        return 2;
    }
    double rel = argc > 2 ? std::atof(argv[2]) : 0.05;
    Extracted ex = extract_file(argv[1]);
    if (!ex.ok) {
        std::cerr << "extract failed: " << ex.err << "\n";
        return 2;
    }
    auto rep = Json::mkobj();
    rep->obj["format"] = Json::mkstr(ex.fmt);
    rep->obj["sheets"] = Json::mknum((double)ex.wb.sheets.size());
    rep->obj["inputs"] = Json::mknum((double)ex.dep.inputs.size());
    rep->obj["dep_edges"] = Json::mknum((double)ex.dep.edges.size());
    rep->obj["sdg_nodes"] = Json::mknum((double)ex.sdg.nodes.size());
    rep->obj["sdg_edges"] = Json::mknum((double)ex.sdg.edges.size());

    Oracle orc(ex.wb);
    orc.build();
    AuditChain chain;
    auto targets = margin_targets(ex.sdg);
    auto arr = Json::mkarr();
    for (const auto& t : targets) {
        Verified v = verify_target(ex.wb, orc, ex.sdg, t, rel, chain);
        if (!v.ok) continue;
        auto o = Json::mkobj();
        o->obj["cell"] = Json::mkstr(ex.wb.key(t.sheet, t.row, t.col));
        o->obj["label"] = Json::mkstr(v.label);
        o->obj["verdict"] = Json::mkstr(verdict_name(v.run.verdict));
        auto enc = Json::mkarr();
        enc->arr.push_back(Json::mknum(v.run.eval.ms.lo));
        enc->arr.push_back(Json::mknum(v.run.eval.ms.hi));
        o->obj["ms_enclosure"] = enc;
        o->obj["inputs"] = Json::mknum((double)v.lowered.input_base.size());
        o->obj["holes"] = Json::mknum((double)v.lowered.holes);
        o->obj["cert"] = Json::mkstr("sha256:" + hex(v.run.cert.digest));
        o->obj["cert_valid"] = Json::mkbool(validate(v.run.cert));
        arr->arr.push_back(o);
    }
    rep->obj["targets"] = arr;
    rep->obj["audit_ok"] = Json::mkbool(chain.verify());
    std::cout << dump(rep) << "\n";
    return 0;
}
