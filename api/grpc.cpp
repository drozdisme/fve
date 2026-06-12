#include "grpc.hpp"
#include "protobuf.hpp"
#include <cstring>

namespace fve {

namespace {

uint32_t be32(const std::string& s, size_t off) {
    return ((uint32_t)(uint8_t)s[off] << 24) | ((uint32_t)(uint8_t)s[off + 1] << 16) |
           ((uint32_t)(uint8_t)s[off + 2] << 8) | (uint8_t)s[off + 3];
}

void put_be32(std::string& s, uint32_t v) {
    s += (char)((v >> 24) & 0xFF);
    s += (char)((v >> 16) & 0xFF);
    s += (char)((v >> 8) & 0xFF);
    s += (char)(v & 0xFF);
}

std::string sfield(const PbField& f) { return std::string(f.bytes.begin(), f.bytes.end()); }

void write_target(PbWriter& w, int field, const JsonP& t) {
    PbWriter tw;
    tw.str(1, t->str("cell"));
    tw.str(2, t->str("label"));
    tw.str(3, t->str("verdict"));
    if (t->has("ms_enclosure") && t->at("ms_enclosure")->arr.size() == 2) {
        tw.dbl(4, t->at("ms_enclosure")->arr[0]->num);
        tw.dbl(5, t->at("ms_enclosure")->arr[1]->num);
    }
    tw.str(6, t->str("method"));
    tw.varint(7, (uint64_t)t->n("holes"));
    tw.str(8, t->str("cert"));
    tw.boolean(9, t->at("cert_valid") && t->at("cert_valid")->b);
    w.message(field, tw);
}

}

bool grpc_unframe(const std::string& body, std::string& msg) {
    if (body.size() < 5) return false;
    uint32_t len = be32(body, 1);
    if (5 + len > body.size()) return false;
    msg = body.substr(5, len);
    return true;
}

std::string grpc_response(const std::string& reply, int status) {
    std::string out;
    out += (char)0x00;
    put_be32(out, (uint32_t)reply.size());
    out += reply;
    std::string trailer = "grpc-status:" + std::to_string(status) + "\r\n";
    out += (char)0x80;
    put_be32(out, (uint32_t)trailer.size());
    out += trailer;
    return out;
}

std::string Grpc::dispatch(const std::string& method, const std::string& msg, int& status) {
    status = 0;
    PbReader rd(msg);
    PbField f;

    if (method == "Upload") {
        std::string name, mime, content;
        while (rd.next(f)) {
            if (f.field == 1) name = sfield(f);
            else if (f.field == 2) mime = sfield(f);
            else if (f.field == 3) content = sfield(f);
        }
        std::vector<uint8_t> bytes(content.begin(), content.end());
        std::string id = svc_.ingest(name.empty() ? "upload.bin" : name, mime, bytes);
        PbWriter w;
        w.str(1, id);
        w.varint(2, bytes.size());
        return w.data();
    }
    if (method == "Analyze") {
        std::string id;
        while (rd.next(f)) if (f.field == 1) id = sfield(f);
        JsonP a = svc_.analyze(id);
        PbWriter w;
        if (a && a->at("ok") && a->at("ok")->b) {
            w.boolean(1, true);
            w.str(2, a->str("model_id"));
            JsonP an = a->at("analysis");
            if (an) {
                w.str(3, an->str("format"));
                w.varint(4, (uint64_t)an->n("sdg_nodes"));
                w.varint(5, (uint64_t)an->n("sdg_edges"));
                w.boolean(6, an->at("glue_consistent") && an->at("glue_consistent")->b);
                w.boolean(7, an->at("glue_obstructed") && an->at("glue_obstructed")->b);
            }
        } else {
            w.boolean(1, false);
        }
        return w.data();
    }
    if (method == "Verify") {
        std::string id;
        double rel = 0.05;
        while (rd.next(f)) {
            if (f.field == 1) id = sfield(f);
            else if (f.field == 2) rel = PbReader::as_double(f.v);
        }
        JsonP v = svc_.verify(id, rel);
        PbWriter w;
        w.boolean(1, v && v->at("ok") && v->at("ok")->b);
        if (v) {
            w.str(2, v->str("run_id"));
            if (v->has("targets"))
                for (auto& t : v->at("targets")->arr) write_target(w, 3, t);
        }
        return w.data();
    }
    if (method == "GetCertificate") {
        std::string id;
        while (rd.next(f)) if (f.field == 1) id = sfield(f);
        JsonP c = svc_.get_certificate(id);
        PbWriter w;
        if (c) {
            w.str(1, c->str("id"));
            w.str(2, c->str("artifact"));
            w.boolean(3, c->at("audit_chain_ok") && c->at("audit_chain_ok")->b);
            if (c->has("targets"))
                for (auto& t : c->at("targets")->arr) write_target(w, 4, t);
        }
        return w.data();
    }
    if (method == "GetArtifact") {
        std::string id;
        while (rd.next(f)) if (f.field == 1) id = sfield(f);
        JsonP m = svc_.get_artifact_meta(id);
        PbWriter w;
        if (m) {
            w.str(1, m->str("id"));
            w.str(2, m->str("name"));
            w.str(3, m->str("hash"));
            w.varint(4, (uint64_t)m->n("size"));
        }
        return w.data();
    }
    if (method == "GetGraph") {
        std::string id;
        while (rd.next(f)) if (f.field == 1) id = sfield(f);
        JsonP g = svc_.get_graph(id);
        PbWriter w;
        if (g && g->at("ok") && g->at("ok")->b) {
            for (auto& n : g->at("nodes")->arr) {
                PbWriter nw;
                nw.str(1, n->str("id"));
                nw.str(2, n->str("label"));
                nw.boolean(3, n->at("input") && n->at("input")->b);
                nw.boolean(4, n->at("ambiguous") && n->at("ambiguous")->b);
                w.message(1, nw);
            }
            for (auto& e : g->at("edges")->arr) {
                PbWriter ew;
                ew.str(1, e->str("from"));
                ew.str(2, e->str("to"));
                ew.str(3, e->str("origin"));
                w.message(2, ew);
            }
        }
        return w.data();
    }
    if (method == "Provenance") {
        std::string id;
        while (rd.next(f)) if (f.field == 1) id = sfield(f);
        JsonP p = svc_.provenance(id);
        PbWriter w;
        if (p)
            for (auto& e : p->arr) {
                PbWriter ew;
                ew.str(1, e->str("from"));
                ew.str(2, e->str("to"));
                ew.str(3, e->str("rel"));
                w.message(1, ew);
            }
        return w.data();
    }
    if (method == "AuditHistory") {
        std::string subj;
        while (rd.next(f)) if (f.field == 1) subj = sfield(f);
        PbWriter w;
        w.boolean(1, svc_.audit_ok());
        JsonP h = svc_.audit_history(subj);
        if (h)
            for (auto& e : h->arr) {
                PbWriter ew;
                ew.varint(1, (uint64_t)e->n("seq"));
                ew.str(2, e->str("type"));
                ew.str(3, e->str("subject"));
                ew.str(4, e->str("prev"));
                w.message(2, ew);
            }
        return w.data();
    }

    status = 12;
    return "";
}

}
