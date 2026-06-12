#include "audit.hpp"

namespace fve {

Audit::Audit(Store& s) : store_(s) { rebuild(); }

void Audit::rebuild() {
    chain_ = AuditChain();
    for (const auto& e : store_.read_log("audit"))
        if (e) chain_.append(sha256(dump(e)));
}

Digest Audit::record(const std::string& type, const std::string& subject, const JsonP& payload,
                     const std::string& user_id) {
    auto e = Json::mkobj();
    e->obj["seq"] = Json::mknum((double)store_.read_log("audit").size());
    e->obj["type"] = Json::mkstr(type);
    e->obj["subject"] = Json::mkstr(subject);
    e->obj["actor"] = Json::mkstr(user_id);
    e->obj["prev"] = Json::mkstr(hex(chain_.head()));
    if (payload) e->obj["payload"] = payload;
    store_.append("audit", e);
    return chain_.append(sha256(dump(e)));
}

std::vector<JsonP> Audit::history(const std::string& subject) const {
    std::vector<JsonP> out;
    for (const auto& e : store_.read_log("audit"))
        if (e && (subject.empty() || e->str("subject") == subject)) out.push_back(e);
    return out;
}

std::vector<JsonP> Audit::lineage(const std::string& artifact) const {
    std::vector<JsonP> out;
    for (const auto& e : store_.read_log("provenance"))
        if (e && (e->str("from") == artifact || e->str("to") == artifact)) out.push_back(e);
    return out;
}

std::vector<JsonP> Audit::certificates() const {
    std::vector<JsonP> out;
    for (const auto& e : store_.read_log("audit"))
        if (e && e->str("type") == "certificate") out.push_back(e);
    return out;
}

bool Audit::verify() const {
    AuditChain c;
    for (const auto& e : store_.read_log("audit"))
        if (e) c.append(sha256(dump(e)));
    return c.verify();
}

}
