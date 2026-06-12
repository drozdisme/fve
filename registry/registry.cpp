#include "registry.hpp"

namespace fve {

namespace {
std::string b64(const std::vector<uint8_t>& d) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    size_t i = 0;
    for (; i + 2 < d.size(); i += 3) {
        uint32_t n = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
        o += t[(n >> 18) & 63];
        o += t[(n >> 12) & 63];
        o += t[(n >> 6) & 63];
        o += t[n & 63];
    }
    if (i < d.size()) {
        uint32_t n = d[i] << 16;
        if (i + 1 < d.size()) n |= d[i + 1] << 8;
        o += t[(n >> 18) & 63];
        o += t[(n >> 12) & 63];
        o += (i + 1 < d.size()) ? t[(n >> 6) & 63] : '=';
        o += '=';
    }
    return o;
}
}

int Registry::version_of(const std::string& kind, const std::string& key) const {
    int v = 0;
    for (const auto& id : store_.list(kind)) {
        JsonP d = store_.get(kind, id);
        if (d && d->str("key") == key) {
            int vv = (int)d->n("version");
            if (vv > v) v = vv;
        }
    }
    return v;
}

std::string Registry::put_artifact(const std::string& name, const std::string& mime,
                                   const std::vector<uint8_t>& bytes, const JsonP& meta) {
    Digest h = sha256(bytes);
    std::string hh = hex(h);
    int ver = version_of("artifacts", name) + 1;
    std::string id = "art_" + hh.substr(0, 16);
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(id);
    d->obj["kind"] = Json::mkstr("artifact");
    d->obj["key"] = Json::mkstr(name);
    d->obj["name"] = Json::mkstr(name);
    d->obj["mime"] = Json::mkstr(mime);
    d->obj["hash"] = Json::mkstr("sha256:" + hh);
    d->obj["version"] = Json::mknum(ver);
    d->obj["size"] = Json::mknum((double)bytes.size());
    d->obj["data"] = Json::mkstr(b64(bytes));
    if (meta) d->obj["meta"] = meta;
    store_.put("artifacts", id, d);
    return id;
}

std::string Registry::put_model(const std::string& artifact_id, const JsonP& model, const JsonP& meta) {
    std::string body = dump(model);
    Digest h = sha256(body);
    std::string hh = hex(h);
    int ver = version_of("models", artifact_id) + 1;
    std::string id = "mdl_" + hh.substr(0, 16);
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(id);
    d->obj["kind"] = Json::mkstr("model");
    d->obj["key"] = Json::mkstr(artifact_id);
    d->obj["artifact"] = Json::mkstr(artifact_id);
    d->obj["hash"] = Json::mkstr("sha256:" + hh);
    d->obj["version"] = Json::mknum(ver);
    d->obj["model"] = model;
    if (meta) d->obj["meta"] = meta;
    store_.put("models", id, d);
    link(artifact_id, id, "produced");
    return id;
}

JsonP Registry::get(const std::string& kind, const std::string& id) const {
    return store_.get(kind, id);
}

std::vector<std::string> Registry::list(const std::string& kind) const {
    return store_.list(kind);
}

void Registry::link(const std::string& from, const std::string& to, const std::string& rel) {
    auto e = Json::mkobj();
    e->obj["from"] = Json::mkstr(from);
    e->obj["to"] = Json::mkstr(to);
    e->obj["rel"] = Json::mkstr(rel);
    store_.append("provenance", e);
}

std::vector<JsonP> Registry::provenance(const std::string& id) const {
    std::vector<JsonP> out;
    for (const auto& e : store_.read_log("provenance"))
        if (e && (e->str("from") == id || e->str("to") == id)) out.push_back(e);
    return out;
}

}
