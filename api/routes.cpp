#include "routes.hpp"
#include "../api/grpc.hpp"
#include "../core/ast/json.hpp"
#include "../platform/event.hpp"
#include <cstdlib>
#include <string>

namespace fve {

void register_routes(Http& http, Service& svc) {

    http.route("POST", "/api/upload", [&](const Req& r) {
        std::string fname = r.header("x-filename");
        std::string mime = r.header("x-mime");
        std::string ct = r.header("content-type");
        std::string bytes = r.body;
        if (ct.find("multipart/form-data") != std::string::npos) {
            std::string fn;
            bytes = multipart_file(r.body, ct, fn);
            if (!fn.empty()) fname = fn;
        }
        if (fname.empty()) fname = "upload.xlsx";
        std::vector<uint8_t> data(bytes.begin(), bytes.end());
        std::string id = svc.ingest(fname, mime.empty() ? "application/octet-stream" : mime, data);
        auto o = Json::mkobj();
        o->obj["artifact_id"] = Json::mkstr(id);
        o->obj["name"] = Json::mkstr(fname);
        o->obj["size"] = Json::mknum((double)data.size());
        return Res::json(dump(o));
    });

    http.route("GET", "/api/artifacts", [&](const Req&) {
        auto a = Json::mkarr();
        for (const auto& id : svc.artifacts()) {
            JsonP m = svc.get_artifact_meta(id);
            if (m) {
                auto o = Json::mkobj();
                o->obj["id"] = Json::mkstr(id);
                o->obj["name"] = m->at("name");
                o->obj["hash"] = m->at("hash");
                a->arr.push_back(o);
            }
        }
        return Res::json(dump(a));
    });

    http.route("GET", "/api/artifacts/:id", [&](const Req& r) {
        JsonP m = svc.get_artifact_meta(r.params.at("id"));
        return m ? Res::json(dump(m)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("POST", "/api/artifacts/:id/analyze", [&](const Req& r) {
        JsonP a = svc.analyze(r.params.at("id"));
        return a ? Res::json(dump(a)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("POST", "/api/artifacts/:id/verify", [&](const Req& r) {
        double rel = 0.05;
        auto it = r.query.find("rel");
        if (it != r.query.end()) rel = std::atof(it->second.c_str());
        JsonP v = svc.verify(r.params.at("id"), rel);
        return v ? Res::json(dump(v)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("GET", "/api/artifacts/:id/recognize", [&](const Req& r) {
        JsonP x = svc.recognize(r.params.at("id"));
        return x ? Res::json(dump(x)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("GET", "/api/artifacts/:id/graph", [&](const Req& r) {
        JsonP g = svc.get_graph(r.params.at("id"));
        return g ? Res::json(dump(g)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("GET", "/api/artifacts/:id/provenance", [&](const Req& r) {
        return Res::json(dump(svc.provenance(r.params.at("id"))));
    });

    http.route("GET", "/api/runs/:id", [&](const Req& r) {
        JsonP c = svc.get_certificate(r.params.at("id"));
        return c ? Res::json(dump(c)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("GET", "/api/audit", [&](const Req& r) {
        std::string subj;
        auto it = r.query.find("subject");
        if (it != r.query.end()) subj = it->second;
        return Res::json(dump(svc.audit_history(subj)));
    });

    http.route("GET", "/api/audit/verify", [&](const Req&) {
        auto o = Json::mkobj();
        o->obj["audit_ok"] = Json::mkbool(svc.audit_ok());
        return Res::json(dump(o));
    });

    http.route("GET", "/api/health", [&](const Req&) {
        return Res::json("{\"status\":\"ok\"}");
    });

    http.route("POST", "/api/l0/crawl", [&](const Req& r) {
        JsonP body = parse(r.body.empty() ? "{}" : r.body);
        std::string root = body && body->has("root") ? body->str("root") : "";
        if (root.empty()) return Res::json("{\"error\":\"root required\"}", 400);
        return Res::json(dump(svc.crawl(root)));
    });
    http.route("GET", "/api/l0/folders", [&](const Req&) { return Res::json(dump(svc.folder_tree())); });
    http.route("GET", "/api/l0/files", [&](const Req& r) {
        int fid = -1;
        auto it = r.query.find("folder");
        if (it != r.query.end()) fid = std::atoi(it->second.c_str());
        return Res::json(dump(svc.registry_files(fid)));
    });
    http.route("POST", "/api/fixes/:id/approve", [&](const Req& r) {
        JsonP b = parse(r.body.empty() ? "{}" : r.body);
        return Res::json(dump(svc.approve_fix(r.params.at("id"), b->str("reviewer_id"))));
    });
    http.route("GET", "/api/coverage/sample", [&](const Req& r) {
        std::string reason; int n = 10;
        auto a = r.query.find("reason"); if (a != r.query.end()) reason = a->second;
        auto b = r.query.find("n"); if (b != r.query.end()) n = std::atoi(b->second.c_str());
        return Res::json(dump(svc.coverage_sample(reason, n)));
    });
    http.route("GET", "/api/coverage/heatmap", [&](const Req& r) { (void)r; return Res::json(dump(svc.unsupported_heatmap())); });
    http.route("GET", "/api/coverage/:id", [&](const Req& r) { return Res::json(dump(svc.coverage(r.params.at("id")))); });

    http.route("GET", "/api/config", [&](const Req& r) { (void)r; return Res::json(dump(svc.config_snapshot())); });

    http.route("POST", "/api/users", [&](const Req& r) {
        JsonP b = parse(r.body.empty() ? "{}" : r.body);
        std::vector<std::string> scope;
        if (b->has("section_scope") && b->at("section_scope")->t == Json::Arr)
            for (const auto& x : b->at("section_scope")->arr) scope.push_back(x->s);
        return Res::json(dump(svc.create_user(b->str("display_name"), b->str("role"), b->str("external_id"), scope)));
    });
    http.route("GET", "/api/users", [&](const Req& r) { (void)r; return Res::json(dump(svc.list_users())); });
    http.route("POST", "/api/sign", [&](const Req& r) {
        JsonP b = parse(r.body.empty() ? "{}" : r.body);
        return Res::json(dump(svc.sign(b->str("user_id"), b->str("subject"), b->str("meaning"), b->has("payload") ? b->at("payload") : nullptr)));
    });
    http.route("GET", "/api/signatures", [&](const Req& r) {
        auto it = r.query.find("subject");
        return Res::json(dump(svc.signatures(it != r.query.end() ? it->second : "")));
    });

    http.route("GET", "/api/cache/stats", [&](const Req& r) {
        (void)r; return Res::json(dump(svc.cache_stats()));
    });

    http.route("POST", "/api/production/report", [&](const Req& r) {
        ProductionEvent ev = parse_event(parse(r.body.empty() ? "{}" : r.body));
        JsonP b = parse(r.body.empty() ? "{}" : r.body);
        if (b->has("issue_type")) ev.defect_type = b->str("issue_type");
        if (b->has("measured_value")) ev.actual_params["measured"] = b->n("measured_value");
        return Res::json(dump(svc.production_report(ev)));
    });
    http.route("GET", "/api/production/report/:id", [&](const Req& r) {
        return Res::json(dump(svc.report_status(r.params.at("id"))));
    });

    http.route("POST", "/api/events", [&](const Req& r) {
        ProductionEvent ev = parse_event(parse(r.body.empty() ? "{}" : r.body));
        return Res::json(dump(svc.handle_event(ev)));
    });
    http.route("GET", "/api/events/:id", [&](const Req& r) {
        JsonP x = svc.get_event(r.params.at("id"));
        return x ? Res::json(dump(x)) : Res::json("{\"error\":\"not found\"}", 404);
    });
    http.route("GET", "/api/events", [&](const Req& r) {
        int pr = 0; auto it = r.query.find("priority"); if (it != r.query.end()) pr = std::atoi(it->second.c_str());
        return Res::json(dump(svc.list_events(pr)));
    });
    http.route("GET", "/api/work_orders", [&](const Req& r) {
        (void)r; return Res::json(dump(svc.list_work_orders()));
    });
    http.route("PUT", "/api/work_orders/:id/resolve", [&](const Req& r) {
        JsonP b = parse(r.body.empty() ? "{}" : r.body);
        std::string fix_id = b->has("fix_id") ? b->str("fix_id") : "";
        std::map<std::string, Ival> patch;
        if (b->has("box_patch") && b->at("box_patch")->t == Json::Obj)
            for (const auto& kv : b->at("box_patch")->obj)
                if (kv.second->arr.size() == 2) patch[kv.first] = Ival{kv.second->arr[0]->num, kv.second->arr[1]->num};
        JsonP x = svc.resolve_work_order(r.params.at("id"), fix_id, patch);
        return x ? Res::json(dump(x)) : Res::json("{\"error\":\"not found\"}", 404);
    });
    http.route("POST", "/api/holes/:id/resolve", [&](const Req& r) {
        JsonP b = parse(r.body.empty() ? "{}" : r.body);
        return Res::json(dump(svc.resolve_hole(r.params.at("id"), b->str("sym"), b->n("lo"), b->n("hi"), b->str("user_id"))));
    });

    http.route("GET", "/api/sections/status", [&](const Req& r) { (void)r; return Res::json(dump(svc.section_status())); });
    http.route("POST", "/api/sections/map", [&](const Req& r) { (void)r; return Res::json(dump(svc.build_section_map())); });

    http.route("GET", "/api/dashboard/production", [&](const Req& r) {
        (void)r; return Res::json(dump(svc.production_dashboard()));
    });

    http.route("GET", "/api/diff", [&](const Req& r) {
        auto a = r.query.find("run_a"); auto b = r.query.find("run_b");
        if (a == r.query.end() || b == r.query.end()) return Res::json("{\"error\":\"run_a,run_b required\"}", 400);
        JsonP x = svc.diff(a->second, b->second);
        return x ? Res::json(dump(x)) : Res::json("{\"error\":\"run not found\"}", 404);
    });
    http.route("GET", "/api/artifacts/:id/history", [&](const Req& r) {
        return Res::json(dump(svc.run_history(r.params.at("id"))));
    });

    http.route("GET", "/api/cases/similar", [&](const Req& r) {
        auto it = r.query.find("artifact");
        if (it == r.query.end()) return Res::json("{\"error\":\"artifact required\"}", 400);
        JsonP x = svc.similar_cases(it->second);
        return x ? Res::json(dump(x)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("POST", "/api/artifacts/:id/admissible_region", [&](const Req& r) {
        double rel = 0.3, eps = 0.01;
        auto a = r.query.find("rel"); if (a != r.query.end()) rel = std::atof(a->second.c_str());
        auto b = r.query.find("eps"); if (b != r.query.end()) eps = std::atof(b->second.c_str());
        JsonP x = svc.admissible_region(r.params.at("id"), rel, eps);
        return x ? Res::json(dump(x)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    http.route("GET", "/api/l0/search", [&](const Req& r) {
        auto it = r.query.find("q");
        std::string q = it != r.query.end() ? it->second : "";
        return Res::json(dump(svc.search(q)));
    });

    http.route("POST", "/api/l0/files/:id/verify", [&](const Req& r) {
        double rel = 0.05;
        auto it = r.query.find("rel");
        if (it != r.query.end()) rel = std::atof(it->second.c_str());
        JsonP v = svc.verify_registered(r.params.at("id"), rel);
        return v ? Res::json(dump(v)) : Res::json("{\"error\":\"not found\"}", 404);
    });

    static Grpc grpc(svc);
    http.route("POST", "/fve.v1.Fve/:method", [&](const Req& r) {
        std::string msg;
        if (!grpc_unframe(r.body, msg)) return Res::text("bad frame", 400);
        int status = 0;
        std::string reply = grpc.dispatch(r.params.at("method"), msg, status);
        Res res;
        res.ctype = "application/grpc-web+proto";
        res.body = grpc_response(reply, status);
        return res;
    });
}

}
