#include <map>
#include <dirent.h>
#include <ctime>
#include "service.hpp"
#include <mutex>
#include "../extractor/pipeline.hpp"
#include "../extractor/ocr/ocr.hpp"
#include "../l0/registry.hpp"
#include "../core/criticality/criticality.hpp"
#include "../platform/coverage.hpp"
#include "../core/sivia/sivia.hpp"
#include "../audit/diff.hpp"
#include "../semantic/glue.hpp"
#include "../wrapper/synth.hpp"
#include "../oracle/runtime.hpp"
#include <sys/stat.h>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <algorithm>
#include <iterator>

namespace fve {

namespace {

std::vector<Section> sections_of(Workbook& wb, const Sdg& sdg) {
    std::vector<Section> secs;
    for (size_t si = 0; si < wb.sheets.size(); si++) {
        Section s;
        s.region = wb.sheets[si].name;
        for (const auto& n : sdg.nodes) {
            if (n.id.sheet != (int)si || n.label.empty()) continue;
            Assign a;
            const Cell* c = wb.sheets[si].at(n.id.row, n.id.col);
            if (c && c->has_value && c->text.empty()) a.range = Ival::point(c->value);
            s.sym[n.label] = a;
        }
        if (!s.sym.empty()) secs.push_back(s);
    }
    return secs;
}

JsonP enc_json(const Ival& v) {
    auto a = Json::mkarr();
    a->arr.push_back(Json::mknum(v.lo));
    a->arr.push_back(Json::mknum(v.hi));
    return a;
}

}

static const char* ENGINE_VERSION = "fve-1.0.0";

Service::Service(const std::string& data_dir)
    : store_(data_dir), reg_(store_), audit_(store_), cb_(store_), ident_(store_) {
    config_hash_ = hex(crit_cfg_.hash());
    if (!store_.exists("config_snapshots", config_hash_)) {
        auto d = Json::mkobj();
        d->obj["hash"] = Json::mkstr(config_hash_);
        d->obj["alpha"] = Json::mknum(crit_cfg_.alpha);
        d->obj["schema_registry_version"] = Json::mkstr(crit_cfg_.schema_version);
        d->obj["canonical"] = Json::mkstr(crit_cfg_.canonical());
        d->obj["engine_version"] = Json::mkstr(ENGINE_VERSION);
        store_.put("config_snapshots", config_hash_, d);
        audit_.record("config_snapshot", config_hash_, d, "system");
    }
}

std::string Service::ext_for(const std::string& name) const {
    size_t d = name.rfind('.');
    std::string e = d == std::string::npos ? "xlsx" : name.substr(d + 1);
    for (char& c : e) c = std::tolower((unsigned char)c);
    return e;
}

std::string Service::blob_path(const std::string& id) const {
    return store_.root() + "/blobs/" + id;
}

std::string Service::ingest(const std::string& name, const std::string& mime,
                            const std::vector<uint8_t>& bytes) {
    std::lock_guard<std::mutex> lk(mu_);
    std::string id = reg_.put_artifact(name, mime, bytes, nullptr);
    ::mkdir((store_.root() + "/blobs").c_str(), 0755);
    std::ofstream f(blob_path(id) + "." + ext_for(name), std::ios::binary);
    f.write((const char*)bytes.data(), bytes.size());
    auto p = Json::mkobj();
    p->obj["name"] = Json::mkstr(name);
    p->obj["bytes"] = Json::mknum((double)bytes.size());
    audit_.record("ingest", id, p);
    return id;
}

JsonP Service::get_artifact_meta(const std::string& artifact_id) const {
    return reg_.get("artifacts", artifact_id);
}

std::vector<std::string> Service::artifacts() const { return reg_.list("artifacts"); }

JsonP Service::analyze(const std::string& artifact_id) {
    std::lock_guard<std::mutex> lk(mu_);
    JsonP meta = reg_.get("artifacts", artifact_id);
    if (!meta) return nullptr;
    std::string path = blob_path(artifact_id) + "." + ext_for(meta->str("name"));
    Extracted ex = extract_file(path);
    auto out = Json::mkobj();
    if (!ex.ok) {
        out->obj["ok"] = Json::mkbool(false);
        out->obj["error"] = Json::mkstr(ex.err);
        return out;
    }
    audit_.record("lift", artifact_id, nullptr);
    audit_.record("interventional_graph", artifact_id, nullptr);

    GlueResult g = glue(sections_of(ex.wb, ex.sdg));

    auto model = Json::mkobj();
    model->obj["format"] = Json::mkstr(ex.fmt);
    model->obj["sheets"] = Json::mknum((double)ex.wb.sheets.size());
    model->obj["inputs"] = Json::mknum((double)ex.dep.inputs.size());
    model->obj["dep_edges"] = Json::mknum((double)ex.dep.edges.size());
    model->obj["sdg_nodes"] = Json::mknum((double)ex.sdg.nodes.size());
    model->obj["sdg_edges"] = Json::mknum((double)ex.sdg.edges.size());
    model->obj["glue_consistent"] = Json::mkbool(g.consistent);
    model->obj["glue_obstructed"] = Json::mkbool(g.obstructed);
    auto conf = Json::mkarr();
    for (const auto& c : g.conflicts) {
        auto o = Json::mkobj();
        o->obj["symbol"] = Json::mkstr(c.symbol);
        o->obj["a"] = Json::mkstr(c.region_a);
        o->obj["b"] = Json::mkstr(c.region_b);
        o->obj["kind"] = Json::mkstr(c.kind);
        conf->arr.push_back(o);
    }
    model->obj["conflicts"] = conf;

    std::string model_id = reg_.put_model(artifact_id, model, nullptr);
    audit_.record("semantic_gluing", artifact_id, conf);

    CoverageLedger cl = compute_coverage(ex.wb);
    persist_coverage(store_, artifact_id, cl);
    auto cov = Json::mkobj();
    cov->obj["total_cells"] = Json::mknum(cl.total_cells);
    cov->obj["formula_cells"] = Json::mknum(cl.formula_cells);
    cov->obj["constant_cells"] = Json::mknum(cl.constant_cells);
    cov->obj["hole_cells"] = Json::mknum(cl.hole_cells);
    cov->obj["confidence"] = Json::mkstr(cl.confidence);
    auto hr = Json::mkobj();
    for (const auto& kv : cl.hole_reasons) hr->obj[kv.first] = Json::mknum(kv.second);
    cov->obj["hole_reasons"] = hr;
    out->obj["coverage"] = cov;

    out->obj["ok"] = Json::mkbool(true);
    out->obj["model_id"] = Json::mkstr(model_id);
    out->obj["analysis"] = model;
    return out;
}

JsonP Service::recognize(const std::string& artifact_id) {
    std::lock_guard<std::mutex> lk(mu_);
    JsonP meta = reg_.get("artifacts", artifact_id);
    if (!meta) return nullptr;
    std::string path = blob_path(artifact_id) + "." + ext_for(meta->str("name"));
    std::ifstream f(path, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> bytes(s.begin(), s.end());
    Raster r = decode_png(bytes);
    auto out = Json::mkobj();
    if (r.w == 0) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr("not a png"); return out; }
    Lattice lat = lattice_from_raster(r);
    std::string formula;
    auto cands = Json::mkarr();
    for (const auto& slot : lat.slots) {
        std::string tok = slot.empty() ? "?" : slot[0].tok;
        formula += tok;
        auto c = Json::mkarr();
        for (const auto& alt : slot) {
            auto a = Json::mkobj();
            a->obj["tok"] = Json::mkstr(alt.tok);
            a->obj["conf"] = Json::mknum(alt.conf);
            c->arr.push_back(a);
        }
        cands->arr.push_back(c);
    }
    Bundle b = bundle_from_lattice(lat, 8, 0.05);
    out->obj["ok"] = Json::mkbool(true);
    out->obj["formula"] = Json::mkstr(formula);
    out->obj["candidates"] = cands;
    out->obj["bundle_size"] = Json::mknum((double)b.size());
    out->obj["ambiguous"] = Json::mkbool(b.ambiguous());
    audit_.record("recognize", artifact_id, out);
    return out;
}

JsonP Service::get_graph(const std::string& artifact_id) {
    JsonP meta = reg_.get("artifacts", artifact_id);
    if (!meta) return nullptr;
    std::string path = blob_path(artifact_id) + "." + ext_for(meta->str("name"));
    Extracted ex = extract_file(path);
    auto out = Json::mkobj();
    if (!ex.ok) { out->obj["ok"] = Json::mkbool(false); return out; }
    auto nodes = Json::mkarr();
    for (const auto& n : ex.sdg.nodes) {
        auto o = Json::mkobj();
        o->obj["id"] = Json::mkstr(ex.wb.key(n.id.sheet, n.id.row, n.id.col));
        o->obj["label"] = Json::mkstr(n.label);
        o->obj["input"] = Json::mkbool(n.is_input);
        o->obj["ambiguous"] = Json::mkbool(n.ambiguous);
        nodes->arr.push_back(o);
    }
    auto edges = Json::mkarr();
    for (const auto& e : ex.sdg.edges) {
        auto o = Json::mkobj();
        o->obj["from"] = Json::mkstr(ex.wb.key(e.from.sheet, e.from.row, e.from.col));
        o->obj["to"] = Json::mkstr(ex.wb.key(e.to.sheet, e.to.row, e.to.col));
        o->obj["origin"] = Json::mkstr(e.origin == Origin::Both ? "both" : e.origin == Origin::Syntactic ? "syntactic" : "interventional");
        edges->arr.push_back(o);
    }
    out->obj["ok"] = Json::mkbool(true);
    out->obj["nodes"] = nodes;
    out->obj["edges"] = edges;
    return out;
}

JsonP Service::verify(const std::string& artifact_id, double rel_dev) {
    std::lock_guard<std::mutex> lk(mu_);
    JsonP meta = reg_.get("artifacts", artifact_id);
    if (!meta) return nullptr;
    std::string path = blob_path(artifact_id) + "." + ext_for(meta->str("name"));
    Extracted ex = extract_file(path);
    auto out = Json::mkobj();
    if (!ex.ok) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr(ex.err); return out; }

    Runtime rt(ex.wb, {});
    RunStats stats;
    rt.run(stats);
    audit_.record("oracle_run", artifact_id, rt.capture());

    Oracle& orc = rt.oracle();
    AuditChain chain;
    auto results = Json::mkarr();
    std::string run_id = "run_" + hex(sha256(artifact_id + std::to_string((long)rel_dev * 1000))).substr(0, 16);

    for (const auto& t : margin_targets(ex.sdg)) {
        Verified v = verify_target(ex.wb, orc, ex.sdg, t, rel_dev, chain);
        if (!v.ok) continue;
        Verdict final_v = v.run.verdict;
        std::string method = v.lowered.holes > 0 ? "interval+holes" : "interval";
        Ival ms = v.run.eval.ms;

        if (final_v == Verdict::Abstain) {
            std::vector<CellId> ins;
            std::set<long long> seen;
            auto key = [](const CellId& a) { return ((long long)a.sheet << 40) | ((long long)a.row << 20) | a.col; };
            for (const auto& e : ex.dep.edges)
                if (e.to.row == t.row && e.to.col == t.col && e.to.sheet == t.sheet)
                    if (seen.insert(key(e.from)).second) ins.push_back(e.from);
            if (!ins.empty() && ins.size() <= 8) {
                ProbeFn pf;
                std::vector<double> base;
                for (const auto& c : ins) {
                    orc.reset();
                    double b = orc.value(c.sheet, c.row, c.col);
                    base.push_back(b);
                    double d = std::fabs(b) * rel_dev;
                    pf.domain.push_back(Ival::of(b - d, b + d));
                }
                pf.f = [&](const std::vector<double>& x) {
                    orc.reset();
                    for (size_t k = 0; k < ins.size(); k++) orc.set(ins[k].sheet, ins[k].row, ins[k].col, x[k]);
                    return orc.value(t.sheet, t.row, t.col);
                };
                int per = ins.size() >= 4 ? 3 : 5;
                LocalModel lm = probe_adaptive(pf, per, 2);
                Ival we = wrap_enclose(lm, pf.domain, 1.25);
                if (!we.is_empty() && !we.is_entire()) {
                    double resid = 0.0;
                    Verdict wv = decide(we.lo, we.hi, resid);
                    if (wv != Verdict::Abstain) {
                        final_v = wv;
                        ms = we;
                        method = "wrapper_synthesis";
                    }
                }
                audit_.record("wrapper_synthesis", artifact_id, nullptr);
            }
        }

        auto o = Json::mkobj();
        o->obj["cell"] = Json::mkstr(ex.wb.key(t.sheet, t.row, t.col));
        o->obj["label"] = Json::mkstr(v.label);
        o->obj["verdict"] = Json::mkstr(verdict_name(final_v));
        o->obj["ms_enclosure"] = enc_json(ms);
        o->obj["method"] = Json::mkstr(method);
        o->obj["holes"] = Json::mknum((double)v.lowered.holes);

        double resid = (method == "wrapper_synthesis") ? 0.0 : v.run.eval.residual;
        EvalResult fr{ms, resid, v.run.eval.trace};
        CritScore cscore = criticality(fr, crit_cfg_);
        double snr_out = std::isfinite(cscore.snr) ? cscore.snr : (cscore.snr < 0 ? -1e9 : 1e9);
        o->obj["cs"] = Json::mknum(cscore.cs);
        o->obj["snr"] = Json::mknum(snr_out);
        o->obj["priority"] = Json::mknum(cscore.priority);
        o->obj["priority_label"] = Json::mkstr(cscore.label);
        o->obj["criticality_why"] = Json::mkstr(cscore.why);
        if (final_v == Verdict::Abstain) {
            AbstainReport ab = explain_abstain(v.run.eval);
            auto reasons = Json::mkarr();
            for (const auto& rr : ab.reasons) {
                auto x = Json::mkobj();
                x->obj["node"] = Json::mkstr(rr.node_id);
                x->obj["label"] = Json::mkstr(rr.node_label);
                x->obj["kind"] = Json::mkstr(rr.kind);
                x->obj["width_share"] = Json::mknum(rr.width_share);
                x->obj["instruction"] = Json::mkstr(rr.instruction);
                reasons->arr.push_back(x);
            }
            auto rep = Json::mkobj();
            rep->obj["summary"] = Json::mkstr(ab.summary);
            rep->obj["reasons"] = reasons;
            o->obj["abstain_report"] = rep;
        }

        o->obj["cert"] = Json::mkstr("sha256:" + hex(v.run.cert.digest));
        o->obj["cert_valid"] = Json::mkbool(validate(v.run.cert));
        results->arr.push_back(o);
        audit_.record("certificate", v.label, o);

        Case kase;
        kase.program_hash = hex(v.run.cert.program_hash);
        kase.artifact_id = artifact_id;
        for (const auto& kv : v.lowered.input_base) {
            double bb = kv.second, dd = std::fabs(bb) * rel_dev;
            if (dd == 0.0) dd = rel_dev > 0 ? rel_dev : 0.1;
            kase.box[kv.first] = Ival::of(bb - dd, bb + dd);
        }
        kase.verdict = final_v;
        kase.cs = cscore.cs;
        kase.cert_hash = "sha256:" + hex(v.run.cert.digest);
        cb_.record(kase);
        if (validate(v.run.cert))
            eval_cache_.put(hex(v.run.cert.program_hash), kase.box, v.run.eval, v.run.cert);
    }

    auto rec = Json::mkobj();
    rec->obj["id"] = Json::mkstr(run_id);
    rec->obj["artifact"] = Json::mkstr(artifact_id);
    rec->obj["rel_dev"] = Json::mknum(rel_dev);
    rec->obj["targets"] = results;
    rec->obj["audit_chain_ok"] = Json::mkbool(chain.verify());
    rec->obj["config_hash"] = Json::mkstr(config_hash_);
    rec->obj["engine_version"] = Json::mkstr(ENGINE_VERSION);
    store_.put("runs", run_id, rec);
    reg_.link(artifact_id, run_id, "verified");
    audit_.record("verify", run_id, nullptr);

    out->obj["ok"] = Json::mkbool(true);
    out->obj["run_id"] = Json::mkstr(run_id);
    out->obj["targets"] = results;
    return out;
}

JsonP Service::get_certificate(const std::string& run_id) const {
    return store_.get("runs", run_id);
}

JsonP Service::provenance(const std::string& id) const {
    auto out = Json::mkarr();
    for (const auto& e : reg_.provenance(id)) out->arr.push_back(e);
    return out;
}

JsonP Service::audit_history(const std::string& subject) const {
    auto out = Json::mkarr();
    for (const auto& e : audit_.history(subject)) out->arr.push_back(e);
    return out;
}

bool Service::audit_ok() const { return audit_.verify(); }

JsonP Service::handle_event(const ProductionEvent& ev) {
    auto out = Json::mkobj();
    out->obj["part_id"] = Json::mkstr(ev.part_id);
    out->obj["section"] = Json::mkstr(ev.section);
    out->obj["defect_type"] = Json::mkstr(ev.defect_type);

    JsonP hits = search(ev.section + " " + ev.defect_type, 1);
    if (!hits || hits->at("matches")->arr.empty()) hits = search(ev.section, 1);
    if (!hits || hits->at("matches")->arr.empty()) {
        out->obj["action"] = Json::mkstr("work_order");
        out->obj["error"] = Json::mkstr("no_file_found");
        return out;
    }
    std::string file_id = hits->at("matches")->arr[0]->str("id");
    JsonP fe = store_.get("file_registry", file_id);
    if (!fe) { out->obj["error"] = Json::mkstr("file_missing"); return out; }

    std::string fpath = fe->str("path");
    std::ifstream f(fpath, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (s.empty()) { out->obj["error"] = Json::mkstr("file_unreadable"); return out; }
    std::vector<uint8_t> bytes(s.begin(), s.end());
    std::string aid = ingest(fe->str("name"), "application/octet-stream", bytes);

    std::string apath = blob_path(aid) + "." + ext_for(fe->str("name"));
    Extracted ex = extract_file(apath);
    if (!ex.ok) { out->obj["error"] = Json::mkstr("extract_failed"); return out; }
    Runtime rt(ex.wb, {});
    RunStats stats;
    rt.run(stats);
    Oracle& orc = rt.oracle();
    auto tgts = margin_targets(ex.sdg);
    if (tgts.empty()) { out->obj["error"] = Json::mkstr("no_target"); return out; }
    Lowered low = lower_target(ex.wb, orc, tgts[0]);
    if (!low.ok) { out->obj["error"] = Json::mkstr("lower_failed"); return out; }

    Program p;
    p.root = low.root;
    for (const auto& kv : low.input_base) {
        double b = kv.second;
        auto pit = ev.actual_params.find(kv.first);
        if (pit != ev.actual_params.end()) {
            double v = pit->second, d = std::fabs(v) * 0.01;
            p.box[kv.first] = Ival::of(v - d, v + d);
        } else {
            double d = std::fabs(b) * 0.05;
            if (d == 0) d = 0.05;
            p.box[kv.first] = Ival::of(b - d, b + d);
        }
    }
    EvalResult r = eval(p);
    CritScore cs = criticality(r, crit_cfg_);
    std::string ph = hex(p.root->hash);
    Verdict verd = decide_eval(r);

    std::string eid = "evt_" + hex(sha256(ev.part_id + ev.section + ev.defect_type + std::to_string((long)store_.list("production_events").size()))).substr(0, 16);
    EventAction action = EventAction::WorkOrder;
    std::string fix_id, wo_id, transfer_cert;

    if (cs.priority >= 4) {
        action = EventAction::Halt;
    } else {
        for (auto& m : cb_.query(ph, p.box, 0.15, 5)) {
            auto tr = cb_.try_transfer(p, m);
            if (tr) {
                action = EventAction::AutoFix;
                fix_id = m.fix.id;
                transfer_cert = "sha256:" + hex(tr->transfer_cert.digest);
                cb_.record_outcome(m.ref.id, m.fix.id, "resolved", transfer_cert);
                break;
            }
        }
    }

    if (action == EventAction::WorkOrder) {
        wo_id = "wo_" + eid.substr(4);
        AbstainReport ab = explain_abstain(r);
        auto wo = Json::mkobj();
        wo->obj["id"] = Json::mkstr(wo_id);
        wo->obj["event_id"] = Json::mkstr(eid);
        wo->obj["status"] = Json::mkstr("open");
        wo->obj["cs"] = Json::mknum(cs.cs);
        wo->obj["priority"] = Json::mknum(cs.priority);
        wo->obj["section"] = Json::mkstr(ev.section);
        wo->obj["file_id"] = Json::mkstr(file_id);
        wo->obj["abstain_why"] = Json::mkstr(verd == Verdict::Abstain ? ab.summary : cs.why);
        store_.put("work_orders", wo_id, wo);
    }

    auto evd = Json::mkobj();
    evd->obj["id"] = Json::mkstr(eid);
    evd->obj["part_id"] = Json::mkstr(ev.part_id);
    evd->obj["section"] = Json::mkstr(ev.section);
    evd->obj["defect_type"] = Json::mkstr(ev.defect_type);
    evd->obj["file_id"] = Json::mkstr(file_id);
    evd->obj["program_hash"] = Json::mkstr(ph);
    evd->obj["verdict"] = Json::mkstr(verdict_name(verd));
    evd->obj["cs"] = Json::mknum(cs.cs);
    evd->obj["priority"] = Json::mknum(cs.priority);
    evd->obj["action"] = Json::mkstr(action_name(action));
    if (!fix_id.empty()) evd->obj["fix_id"] = Json::mkstr(fix_id);
    if (!wo_id.empty()) evd->obj["work_order_id"] = Json::mkstr(wo_id);
    if (!ev.operator_id.empty()) evd->obj["reported_by"] = Json::mkstr(ev.operator_id);
    if (!ev.voice_note_id.empty()) evd->obj["voice_note_id"] = Json::mkstr(ev.voice_note_id);
    if (!ev.photo_id.empty()) evd->obj["photo_id"] = Json::mkstr(ev.photo_id);
    evd->obj["config_hash"] = Json::mkstr(config_hash_);
    store_.put("production_events", eid, evd);
    audit_.record("production_event", eid, evd, ev.operator_id.empty() ? "system" : ev.operator_id);

    out->obj["event_id"] = Json::mkstr(eid);
    out->obj["action"] = Json::mkstr(action_name(action));
    out->obj["verdict"] = Json::mkstr(verdict_name(verd));
    out->obj["cs"] = Json::mknum(cs.cs);
    out->obj["priority"] = Json::mknum(cs.priority);
    out->obj["priority_label"] = Json::mkstr(cs.label);
    if (!fix_id.empty()) { out->obj["fix_id"] = Json::mkstr(fix_id); out->obj["transfer_cert"] = Json::mkstr(transfer_cert); }
    if (!wo_id.empty()) out->obj["work_order_id"] = Json::mkstr(wo_id);
    out->obj["ok"] = Json::mkbool(true);
    return out;
}

JsonP Service::get_event(const std::string& id) const {
    return store_.get("production_events", id);
}

JsonP Service::list_events(int min_priority) const {
    auto arr = Json::mkarr();
    for (const auto& id : store_.list("production_events")) {
        JsonP e = store_.get("production_events", id);
        if (e && (int)e->n("priority") >= min_priority) arr->arr.push_back(e);
    }
    auto out = Json::mkobj();
    out->obj["events"] = arr;
    return out;
}

JsonP Service::list_work_orders() const {
    auto arr = Json::mkarr();
    for (const auto& id : store_.list("work_orders")) {
        JsonP w = store_.get("work_orders", id);
        if (w) arr->arr.push_back(w);
    }
    auto out = Json::mkobj();
    out->obj["work_orders"] = arr;
    return out;
}

JsonP Service::resolve_work_order(const std::string& wo_id, const std::string& fix_id, const std::map<std::string, Ival>& patch) {
    JsonP w = store_.get("work_orders", wo_id);
    if (!w) return nullptr;
    Fix fx;
    fx.id = fix_id.empty() ? ("fix_" + wo_id) : fix_id;
    fx.description = "engineer fix for " + wo_id;
    fx.box_patch = patch;
    cb_.record_fix(fx);
    w->obj["status"] = Json::mkstr("resolved");
    w->obj["fix_id"] = Json::mkstr(fx.id);
    store_.put("work_orders", wo_id, w);
    audit_.record("work_order_resolved", wo_id, w);
    return w;
}

namespace {
void apply_light(JsonP out, int priority, const std::string& event_id) {
    std::string light, status, message;
    JsonP eta = Json::mknull();
    if (priority <= 1) {
        light = "green"; status = "PROCEED"; message = "Продолжайте";
    } else if (priority == 2) {
        light = "yellow"; status = "REVIEW"; message = "Проверено, можно продолжать";
    } else {
        light = "red"; status = "STOP"; message = "Стоп — инженер уведомлён";
        eta = Json::mknum(priority >= 4 ? 25 : 15);
    }
    out->obj["light"] = Json::mkstr(light);
    out->obj["status"] = Json::mkstr(status);
    out->obj["message"] = Json::mkstr(message);
    out->obj["eta_minutes"] = eta;
    out->obj["event_id"] = Json::mkstr(event_id);
}
}

JsonP Service::production_report(const ProductionEvent& ev_in) {
    ProductionEvent ev = ev_in;
    if (ev.section.empty()) ev.section = ev.part_id;
    JsonP r = handle_event(ev);
    auto out = Json::mkobj();
    int priority = r && r->has("priority") ? (int)r->n("priority") : 3;
    bool ok = r && r->has("ok") && r->at("ok")->b;
    if (!ok) {
        out->obj["light"] = Json::mkstr("red");
        out->obj["status"] = Json::mkstr("STOP");
        out->obj["message"] = Json::mkstr("Не удалось сопоставить деталь — требуется инженер");
        out->obj["eta_minutes"] = Json::mknum(20);
        out->obj["event_id"] = Json::mkstr(r && r->has("event_id") ? r->str("event_id") : "");
        return out;
    }
    apply_light(out, priority, r->str("event_id"));
    if (r->has("work_order_id")) out->obj["work_order_id"] = r->at("work_order_id");
    return out;
}

JsonP Service::report_status(const std::string& event_id) const {
    JsonP e = store_.get("production_events", event_id);
    auto out = Json::mkobj();
    if (!e) { out->obj["light"] = Json::mkstr("pending"); out->obj["message"] = Json::mkstr("Проверяем..."); return out; }
    apply_light(out, (int)e->n("priority"), event_id);
    return out;
}

bool Service::registry_empty() const { return store_.list("file_registry").empty(); }

std::vector<std::string> Service::head_file_ids() const {
    std::vector<std::string> v;
    for (const auto& id : store_.list("file_registry")) {
        JsonP e = store_.get("file_registry", id);
        if (e && e->has("is_head") && e->at("is_head")->b) v.push_back(id);
    }
    return v;
}

bool Service::users_empty() const { return ident_.list_users().empty(); }

std::string Service::audit_head() const { return audit_.head(); }

JsonP Service::bootstrap_admin() {
    auto out = Json::mkobj();
    if (!users_empty()) { out->obj["exists"] = Json::mkbool(true); return out; }
    unsigned char raw[32];
    std::string token;
    std::ifstream ur("/dev/urandom", std::ios::binary);
    if (ur.read((char*)raw, 32)) token = hex(sha256(std::string((char*)raw, 32) + std::to_string((long)::time(nullptr))));
    else token = hex(sha256("fallback" + std::to_string((long)::time(nullptr))));
    User admin;
    admin.display_name = "admin";
    admin.role = "admin";
    admin.api_token_hash = hex(sha256(token));
    std::string id = ident_.create_user(admin);
    audit_.record("identity_bootstrap", id, nullptr, "system");
    out->obj["exists"] = Json::mkbool(false);
    out->obj["user_id"] = Json::mkstr(id);
    out->obj["token"] = Json::mkstr(token);
    return out;
}

JsonP Service::apply_migrations(const std::string& dir) {
    auto out = Json::mkobj();
    auto applied = Json::mkarr();
    int n = 0;
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        std::vector<std::string> files;
        dirent* e;
        while ((e = ::readdir(d)) != nullptr) {
            std::string nm = e->d_name;
            if (nm.size() > 4 && nm.substr(nm.size() - 4) == ".sql") files.push_back(nm);
        }
        ::closedir(d);
        std::sort(files.begin(), files.end());
        for (const auto& nm : files) {
            if (store_.exists("_migrations", nm)) continue;
            auto m = Json::mkobj();
            m->obj["name"] = Json::mkstr(nm);
            m->obj["applied_at"] = Json::mknum((double)::time(nullptr));
            store_.put("_migrations", nm, m);
            applied->arr.push_back(Json::mkstr(nm));
            n++;
        }
    }
    out->obj["newly_applied"] = applied;
    out->obj["count"] = Json::mknum(n);
    return out;
}

JsonP Service::crawl_summary() const {
    std::map<std::string, long> by_format, by_verdict, top_skip;
    long total = 0, total_cells = 0, formula_cells = 0;
    for (const auto& id : store_.list("file_registry")) {
        JsonP e = store_.get("file_registry", id);
        if (!e || !(e->has("is_head") && e->at("is_head")->b)) continue;
        total++;
        by_format[e->str("format_type")]++;
    }
    for (const auto& id : store_.list("runs")) {
        JsonP r = store_.get("runs", id);
        if (!r || !r->has("targets")) continue;
        for (const auto& t : r->at("targets")->arr) by_verdict[t->str("verdict")]++;
    }
    for (const auto& id : store_.list("coverage_ledger")) {
        JsonP c = store_.get("coverage_ledger", id);
        if (!c) continue;
        total_cells += (long)c->n("total_cells");
        formula_cells += (long)c->n("formula_cells");
        if (c->has("hole_reasons"))
            for (const auto& kv : c->at("hole_reasons")->obj) top_skip[kv.first] += (long)kv.second->num;
    }
    auto out = Json::mkobj();
    out->obj["total_files"] = Json::mknum((double)total);
    auto bf = Json::mkobj();
    for (const auto& kv : by_format) bf->obj[kv.first] = Json::mknum((double)kv.second);
    out->obj["by_format"] = bf;
    auto bv = Json::mkobj();
    for (const auto& kv : by_verdict) bv->obj[kv.first] = Json::mknum((double)kv.second);
    out->obj["by_verdict"] = bv;
    std::vector<std::pair<std::string, long>> sk(top_skip.begin(), top_skip.end());
    std::sort(sk.begin(), sk.end(), [](const std::pair<std::string, long>& a, const std::pair<std::string, long>& b) { return a.second > b.second; });
    auto ts = Json::mkarr();
    for (size_t i = 0; i < sk.size() && i < 10; i++) {
        auto o = Json::mkobj();
        o->obj["construct"] = Json::mkstr(sk[i].first);
        o->obj["cell_count"] = Json::mknum((double)sk[i].second);
        ts->arr.push_back(o);
    }
    out->obj["top_skip_reasons"] = ts;
    out->obj["coverage_pct"] = Json::mknum(total_cells > 0 ? 100.0 * formula_cells / total_cells : 0.0);
    return out;
}

JsonP Service::resolve_hole(const std::string& hole_id, const std::string& sym, double lo, double hi, const std::string& user_id) {
    Fix fx;
    fx.id = "fix_" + hole_id;
    fx.description = "bound hole " + hole_id + " on " + sym;
    fx.box_patch = {{sym, Ival::of(lo, hi)}};
    cb_.record_fix(fx); // status 'proposed' until a reviewer approves
    audit_.record("hole_resolution_proposed", hole_id, nullptr, user_id.empty() ? "system" : user_id);
    auto o = Json::mkobj();
    o->obj["ok"] = Json::mkbool(true);
    o->obj["fix_id"] = Json::mkstr(fx.id);
    o->obj["status"] = Json::mkstr("proposed");
    o->obj["note"] = Json::mkstr("requires reviewer approval before automatic transfer");
    return o;
}

JsonP Service::build_section_map() {
    auto out = Json::mkobj();
    auto arr = Json::mkarr();
    int n = 0;
    for (const auto& id : store_.list("folders")) {
        JsonP f = store_.get("folders", id);
        if (!f) continue;
        std::string rel = f->str("rel");
        if (rel.empty()) continue;
        auto d = Json::mkobj();
        d->obj["folder_id"] = Json::mkstr(id);
        d->obj["section_name"] = Json::mkstr(rel);
        store_.put("section_map", id, d);
        arr->arr.push_back(d);
        n++;
    }
    out->obj["sections"] = arr;
    out->obj["count"] = Json::mknum(n);
    return out;
}

JsonP Service::section_status() const {
    struct Agg { int files = 0; int max_priority = -1; std::string domain; long last = 0; };
    std::map<std::string, Agg> by_section;
    auto dirname = [](const std::string& rel) {
        size_t p = rel.rfind('/');
        return p == std::string::npos ? std::string("(root)") : rel.substr(0, p);
    };
    for (const auto& id : store_.list("file_registry")) {
        JsonP e = store_.get("file_registry", id);
        if (!e || !(e->has("is_head") && e->at("is_head")->b)) continue;
        std::string sec = dirname(e->str("rel_path"));
        Agg& a = by_section[sec];
        a.files++;
        if (a.domain.empty()) a.domain = e->str("domain");
        long mt = (long)e->n("mtime");
        if (mt > a.last) a.last = mt;
    }
    for (const auto& id : store_.list("production_events")) {
        JsonP ev = store_.get("production_events", id);
        if (!ev) continue;
        std::string sec = ev->str("section");
        // match event section against known section keys (prefix)
        for (auto& kv : by_section)
            if (sec == kv.first || sec.rfind(kv.first, 0) == 0 || kv.first.rfind(sec, 0) == 0) {
                int pr = (int)ev->n("priority");
                if (pr > kv.second.max_priority) kv.second.max_priority = pr;
            }
    }
    auto arr = Json::mkarr();
    for (const auto& kv : by_section) {
        auto o = Json::mkobj();
        o->obj["section"] = Json::mkstr(kv.first);
        o->obj["files"] = Json::mknum(kv.second.files);
        o->obj["domain"] = Json::mkstr(kv.second.domain);
        o->obj["max_priority"] = Json::mknum(kv.second.max_priority);
        o->obj["last_updated"] = Json::mknum((double)kv.second.last);
        arr->arr.push_back(o);
    }
    auto out = Json::mkobj();
    out->obj["sections"] = arr;
    return out;
}

JsonP Service::production_dashboard() const {
    int halt = 0, crit = 0, review = 0, watch = 0, safe = 0, open_wo = 0;
    for (const auto& id : store_.list("production_events")) {
        JsonP e = store_.get("production_events", id);
        if (!e) continue;
        switch ((int)e->n("priority")) {
            case 4: halt++; break;
            case 3: crit++; break;
            case 2: review++; break;
            case 1: watch++; break;
            default: safe++; break;
        }
    }
    for (const auto& id : store_.list("work_orders")) {
        JsonP w = store_.get("work_orders", id);
        if (w && w->str("status") == "open") open_wo++;
    }
    auto out = Json::mkobj();
    out->obj["halt"] = Json::mknum(halt);
    out->obj["critical"] = Json::mknum(crit);
    out->obj["review"] = Json::mknum(review);
    out->obj["watch"] = Json::mknum(watch);
    out->obj["safe"] = Json::mknum(safe);
    out->obj["open_work_orders"] = Json::mknum(open_wo);
    return out;
}

JsonP Service::create_user(const std::string& display_name, const std::string& role,
                           const std::string& external_id, const std::vector<std::string>& scope) {
    User u;
    u.display_name = display_name;
    u.role = role;
    u.external_id = external_id;
    u.section_scope = scope;
    std::string id = ident_.create_user(u);
    audit_.record("user_created", id, nullptr, "system");
    auto o = Json::mkobj();
    o->obj["id"] = Json::mkstr(id);
    o->obj["role"] = Json::mkstr(role);
    return o;
}

JsonP Service::list_users() const {
    auto arr = Json::mkarr();
    for (const auto& u : ident_.list_users()) arr->arr.push_back(u);
    auto o = Json::mkobj();
    o->obj["users"] = arr;
    return o;
}

JsonP Service::sign(const std::string& user_id, const std::string& subject, const std::string& meaning, const JsonP& payload) {
    if (!ident_.active(user_id)) {
        auto e = Json::mkobj();
        e->obj["ok"] = Json::mkbool(false);
        e->obj["error"] = Json::mkstr("unknown_or_inactive_user");
        return e;
    }
    std::string sid = ident_.sign(user_id, subject, meaning, payload);
    audit_.record("signature", subject, nullptr, user_id);
    auto o = Json::mkobj();
    o->obj["ok"] = Json::mkbool(true);
    o->obj["signature_id"] = Json::mkstr(sid);
    return o;
}

JsonP Service::signatures(const std::string& subject) const {
    auto arr = Json::mkarr();
    for (const auto& s : ident_.signatures(subject)) arr->arr.push_back(s);
    auto o = Json::mkobj();
    o->obj["signatures"] = arr;
    return o;
}

JsonP Service::approve_fix(const std::string& fix_id, const std::string& reviewer_user_id) {
    auto out = Json::mkobj();
    if (!ident_.has_role(reviewer_user_id, "reviewer")) {
        out->obj["ok"] = Json::mkbool(false);
        out->obj["error"] = Json::mkstr("requires_reviewer_role");
        return out;
    }
    JsonP fj = store_.get("fixes", fix_id);
    if (!fj) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr("fix_not_found"); return out; }
    ident_.sign(reviewer_user_id, fix_id, "approved_fix", fj);
    cb_.approve_fix(fix_id, reviewer_user_id);
    audit_.record("fix_approved", fix_id, nullptr, reviewer_user_id);
    out->obj["ok"] = Json::mkbool(true);
    out->obj["fix_id"] = Json::mkstr(fix_id);
    out->obj["status"] = Json::mkstr("approved");
    return out;
}

JsonP Service::config_snapshot() const {
    JsonP d = store_.get("config_snapshots", config_hash_);
    return d ? d : Json::mkobj();
}

JsonP Service::coverage(const std::string& artifact_id) const {
    JsonP d = store_.get("coverage_ledger", artifact_id);
    if (d) return d;
    auto o = Json::mkobj();
    o->obj["ok"] = Json::mkbool(false);
    o->obj["error"] = Json::mkstr("no coverage — run analyze first");
    return o;
}

JsonP Service::coverage_sample(const std::string& reason, int n) const {
    std::vector<JsonP> pool;
    for (const auto& id : store_.list("coverage_ledger")) {
        JsonP d = store_.get("coverage_ledger", id);
        if (!d || !d->has("samples")) continue;
        for (const auto& s : d->at("samples")->arr)
            if (reason.empty() || s->str("reason") == reason ||
                s->str("reason").rfind(reason, 0) == 0)
                pool.push_back(s);
    }
    // deterministic pseudo-random selection without external rng
    auto out = Json::mkobj();
    auto arr = Json::mkarr();
    size_t step = pool.empty() ? 1 : std::max<size_t>(1, pool.size() / (size_t)std::max(1, n));
    for (size_t i = 0, taken = 0; i < pool.size() && (int)taken < n; i += step, taken++)
        arr->arr.push_back(pool[i]);
    out->obj["reason"] = Json::mkstr(reason);
    out->obj["total_available"] = Json::mknum((double)pool.size());
    out->obj["samples"] = arr;
    return out;
}

JsonP Service::unsupported_heatmap() const {
    std::vector<JsonP> rows;
    for (const auto& id : store_.list("unsupported_constructs")) {
        JsonP d = store_.get("unsupported_constructs", id);
        if (d) rows.push_back(d);
    }
    std::sort(rows.begin(), rows.end(),
              [](const JsonP& a, const JsonP& b) { return a->n("cell_count") > b->n("cell_count"); });
    auto out = Json::mkobj();
    auto arr = Json::mkarr();
    for (const auto& r : rows) arr->arr.push_back(r);
    out->obj["constructs"] = arr;
    return out;
}

JsonP Service::cache_stats() const {
    auto o = Json::mkobj();
    o->obj["size"] = Json::mknum((double)eval_cache_.size());
    return o;
}

JsonP Service::diff(const std::string& run_a, const std::string& run_b) {
    CertDiff d = diff_runs(store_, run_a, run_b);
    if (!d.ok) return nullptr;
    return cert_diff_json(d);
}

JsonP Service::run_history(const std::string& artifact_id) const {
    auto arr = Json::mkarr();
    auto ids = store_.list("runs");
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        JsonP r = store_.get("runs", id);
        if (!r || r->str("artifact") != artifact_id) continue;
        int worst = 0; // 0 safe,1 abstain,2 fail
        double max_cs = 0.0;
        int n = 0;
        if (r->has("targets"))
            for (const auto& t : r->at("targets")->arr) {
                n++;
                std::string v = t->str("verdict");
                int rank = v == "FAIL" ? 2 : v == "ABSTAIN" ? 1 : 0;
                if (rank > worst) worst = rank;
                if (t->has("cs")) max_cs = std::max(max_cs, t->n("cs"));
            }
        auto e = Json::mkobj();
        e->obj["run_id"] = Json::mkstr(id);
        e->obj["n_targets"] = Json::mknum(n);
        e->obj["verdict"] = Json::mkstr(worst == 2 ? "FAIL" : worst == 1 ? "ABSTAIN" : "SAFE");
        e->obj["max_cs"] = Json::mknum(max_cs);
        arr->arr.push_back(e);
    }
    auto out = Json::mkobj();
    out->obj["artifact"] = Json::mkstr(artifact_id);
    out->obj["runs"] = arr;
    return out;
}

namespace {
JsonP box_to_json(const Box& b) {
    auto o = Json::mkobj();
    for (const auto& kv : b) {
        auto iv = Json::mkarr();
        iv->arr.push_back(Json::mknum(kv.second.lo));
        iv->arr.push_back(Json::mknum(kv.second.hi));
        o->obj[kv.first] = iv;
    }
    return o;
}
}

JsonP Service::similar_cases(const std::string& artifact_id, double rel_dev) {
    std::lock_guard<std::mutex> lk(mu_);
    JsonP meta = reg_.get("artifacts", artifact_id);
    if (!meta) return nullptr;
    std::string path = blob_path(artifact_id) + "." + ext_for(meta->str("name"));
    Extracted ex = extract_file(path);
    auto out = Json::mkobj();
    if (!ex.ok) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr(ex.err); return out; }
    Runtime rt(ex.wb, {});
    RunStats stats;
    rt.run(stats);
    Oracle& orc = rt.oracle();
    auto targets = margin_targets(ex.sdg);
    if (targets.empty()) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr("no margin target"); return out; }
    Lowered low = lower_target(ex.wb, orc, targets[0]);
    if (!low.ok) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr("lowering failed"); return out; }
    Program p;
    p.root = low.root;
    for (auto& kv : low.input_base) {
        double b = kv.second, d = std::fabs(b) * rel_dev;
        if (d == 0.0) d = rel_dev > 0 ? rel_dev : 0.1;
        p.box[kv.first] = Ival::of(b - d, b + d);
    }
    std::string ph = hex(p.root->hash);
    auto matches = cb_.query(ph, p.box, 0.15, 5);
    auto arr = Json::mkarr();
    for (auto& m : matches) {
        auto mj = Json::mkobj();
        mj->obj["case_id"] = Json::mkstr(m.ref.id);
        mj->obj["distance"] = Json::mknum(m.distance);
        mj->obj["fix_id"] = Json::mkstr(m.fix.id);
        mj->obj["fix_description"] = Json::mkstr(m.fix.description);
        auto transferred = cb_.try_transfer(p, m);
        mj->obj["transfer_verified"] = Json::mkbool(transferred.has_value());
        if (transferred) mj->obj["transfer_verdict"] = Json::mkstr("SAFE");
        arr->arr.push_back(mj);
    }
    out->obj["ok"] = Json::mkbool(true);
    out->obj["program_hash"] = Json::mkstr(ph);
    out->obj["matches"] = arr;
    return out;
}

JsonP Service::admissible_region(const std::string& artifact_id, double rel_dev, double eps) {
    std::lock_guard<std::mutex> lk(mu_);
    JsonP meta = reg_.get("artifacts", artifact_id);
    if (!meta) return nullptr;
    std::string path = blob_path(artifact_id) + "." + ext_for(meta->str("name"));
    Extracted ex = extract_file(path);
    auto out = Json::mkobj();
    if (!ex.ok) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr(ex.err); return out; }
    Runtime rt(ex.wb, {});
    RunStats stats;
    rt.run(stats);
    Oracle& orc = rt.oracle();

    auto targets = margin_targets(ex.sdg);
    if (targets.empty()) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr("no margin target"); return out; }
    CellId t = targets[0];
    Lowered low = lower_target(ex.wb, orc, t);
    if (!low.ok) { out->obj["ok"] = Json::mkbool(false); out->obj["error"] = Json::mkstr("lowering failed"); return out; }

    Program p;
    p.root = low.root;
    for (auto& kv : low.input_base) {
        double b = kv.second;
        double d = std::fabs(b) * rel_dev;
        if (d == 0.0) d = rel_dev > 0 ? rel_dev : 0.1;
        p.box[kv.first] = Ival::of(b - d, b + d);
    }
    SiviaConfig cfg;
    cfg.epsilon = eps;
    SiviaResult sr = sivia(p, p.box, cfg);

    auto sboxes = Json::mkarr();
    for (size_t i = 0; i < sr.safe_boxes.size() && i < 500; i++) sboxes->arr.push_back(box_to_json(sr.safe_boxes[i]));
    auto uboxes = Json::mkarr();
    for (size_t i = 0; i < sr.unknown_boxes.size() && i < 500; i++) uboxes->arr.push_back(box_to_json(sr.unknown_boxes[i]));

    const SdgNode* nd = ex.sdg.find(t);
    out->obj["ok"] = Json::mkbool(true);
    out->obj["target"] = Json::mkstr(nd && !nd->label.empty() ? nd->label : ex.wb.key(t.sheet, t.row, t.col));
    out->obj["dims"] = Json::mknum((double)p.box.size());
    out->obj["safe_fraction"] = Json::mknum(sr.safe_volume_fraction);
    out->obj["iterations"] = Json::mknum(sr.iterations);
    out->obj["n_safe"] = Json::mknum((double)sr.safe_boxes.size());
    out->obj["n_unknown"] = Json::mknum((double)sr.unknown_boxes.size());
    out->obj["safe_boxes"] = sboxes;
    out->obj["unknown_boxes"] = uboxes;

    std::string sid = "sivia_" + hex(sha256(artifact_id + std::to_string(eps))).substr(0, 16);
    store_.put("sivia_runs", sid, out);
    audit_.record("admissible_region", artifact_id, nullptr);
    return out;
}

JsonP Service::crawl(const std::string& root) {
    std::lock_guard<std::mutex> lk(mu_);
    Catalog reg = build_registry(root);
    migrate(reg, store_);
    int heads = 0;
    for (const auto& e : reg.files) if (e.is_head) heads++;
    auto out = Json::mkobj();
    out->obj["ok"] = Json::mkbool(true);
    out->obj["root"] = Json::mkstr(root);
    out->obj["folders"] = Json::mknum((double)reg.folders.size());
    out->obj["files"] = Json::mknum((double)reg.files.size());
    out->obj["heads"] = Json::mknum(heads);
    audit_.record("l0_crawl", root, out);
    return out;
}

JsonP Service::folder_tree() const {
    auto arr = Json::mkarr();
    auto ids = store_.list("folders");
    std::sort(ids.begin(), ids.end());
    for (const auto& id : ids) {
        JsonP d = store_.get("folders", id);
        if (d) arr->arr.push_back(d);
    }
    return arr;
}

JsonP Service::registry_files(int folder_id) const {
    auto arr = Json::mkarr();
    for (const auto& id : store_.list("file_registry")) {
        JsonP d = store_.get("file_registry", id);
        if (!d) continue;
        if (folder_id >= 0 && (int)d->n("folder_id") != folder_id) continue;
        arr->arr.push_back(d);
    }
    return arr;
}

JsonP Service::search(const std::string& query, int limit) const {
    std::string q = query;
    for (char& c : q) c = (char)std::tolower((unsigned char)c);
    std::vector<std::string> words;
    std::string w;
    for (char c : q) {
        if (c == ' ' || c == '_' || c == '-' || c == '.' || c == ',') { if (!w.empty()) { words.push_back(w); w.clear(); } }
        else w += c;
    }
    if (!w.empty()) words.push_back(w);

    struct Hit { JsonP doc; int score; long mtime; };
    std::vector<Hit> hits;
    for (const auto& id : store_.list("file_registry")) {
        JsonP d = store_.get("file_registry", id);
        if (!d || !(d->at("is_head") && d->at("is_head")->b)) continue;
        std::string hay = d->str("config_key") + " " + d->str("name") + " " + d->str("rel_path") + " " +
                          d->str("format_type") + " " + d->str("domain") + " " + d->str("keywords") + " " +
                          d->str("revision");
        for (char& c : hay) c = (char)std::tolower((unsigned char)c);
        bool all = true;
        int score = 0;
        for (const auto& word : words) {
            size_t pos = hay.find(word);
            if (pos == std::string::npos) { all = false; break; }
            score += (d->str("config_key").find(word) != std::string::npos) ? 3 : 1;
        }
        if (all && !words.empty()) hits.push_back({d, score + (int)d->n("marker_hits"), (long)d->n("mtime")});
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.mtime > b.mtime;
    });
    auto out = Json::mkobj();
    out->obj["query"] = Json::mkstr(query);
    auto arr = Json::mkarr();
    int n = 0;
    for (const auto& h : hits) {
        if (n++ >= limit) break;
        auto r = Json::mkobj();
        r->obj["id"] = h.doc->at("id");
        r->obj["name"] = h.doc->at("name");
        r->obj["rel_path"] = h.doc->at("rel_path");
        r->obj["format_type"] = h.doc->at("format_type");
        r->obj["domain"] = h.doc->at("domain");
        r->obj["revision"] = h.doc->at("revision");
        r->obj["rev_rank"] = h.doc->at("rev_rank");
        r->obj["version_group"] = h.doc->at("version_group");
        r->obj["is_head"] = Json::mkbool(true);
        arr->arr.push_back(r);
    }
    out->obj["matches"] = arr;
    out->obj["count"] = Json::mknum((double)hits.size());
    if (!hits.empty()) out->obj["newest"] = arr->arr.empty() ? Json::mknull() : arr->arr[0];
    return out;
}

JsonP Service::verify_registered(const std::string& file_id, double rel_dev) {
    JsonP e = store_.get("file_registry", file_id);
    if (!e) return nullptr;
    std::string path = e->str("path");
    std::ifstream f(path, std::ios::binary);
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (s.empty()) {
        auto out = Json::mkobj();
        out->obj["ok"] = Json::mkbool(false);
        out->obj["error"] = Json::mkstr("file unreadable");
        return out;
    }
    std::vector<uint8_t> bytes(s.begin(), s.end());
    std::string aid = ingest(e->str("name"), "application/octet-stream", bytes);
    reg_.link(file_id, aid, "registered_as");
    JsonP v = verify(aid, rel_dev);
    if (v) {
        v->obj["artifact_id"] = Json::mkstr(aid);
        v->obj["file_id"] = Json::mkstr(file_id);
        v->obj["format_type"] = Json::mkstr(e->str("format_type"));
        v->obj["rel_path"] = Json::mkstr(e->str("rel_path"));
    }
    return v;
}

}
