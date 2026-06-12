#include "identity.hpp"
#include "../core/merkle/sha256.hpp"
#include <ctime>

namespace fve {

std::string Identity::create_user(const User& u) {
    std::string id = u.id.empty()
                         ? "user_" + hex(sha256(u.external_id + "|" + u.display_name)).substr(0, 16)
                         : u.id;
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(id);
    d->obj["external_id"] = Json::mkstr(u.external_id);
    d->obj["display_name"] = Json::mkstr(u.display_name);
    d->obj["role"] = Json::mkstr(u.role);
    auto sc = Json::mkarr();
    for (const auto& s : u.section_scope) sc->arr.push_back(Json::mkstr(s));
    d->obj["section_scope"] = sc;
    d->obj["active"] = Json::mkbool(u.active);
    d->obj["api_token_hash"] = Json::mkstr(u.api_token_hash);
    store_.put("users", id, d);
    return id;
}

JsonP Identity::get_user(const std::string& id) const {
    return store_.get("users", id);
}

std::vector<JsonP> Identity::list_users() const {
    std::vector<JsonP> v;
    for (const auto& id : store_.list("users")) {
        JsonP u = store_.get("users", id);
        if (u) v.push_back(u);
    }
    return v;
}

bool Identity::has_role(const std::string& user_id, const std::string& role) const {
    JsonP u = get_user(user_id);
    if (!u || (u->has("active") && !u->at("active")->b)) return false;
    std::string r = u->str("role");
    if (r == "admin") return true;
    return r == role;
}

bool Identity::active(const std::string& user_id) const {
    JsonP u = get_user(user_id);
    return u && (!u->has("active") || u->at("active")->b);
}

std::string Identity::sign(const std::string& user_id, const std::string& subject,
                           const std::string& meaning, const JsonP& payload) {
    long ts = (long)std::time(nullptr);
    std::string id = "sig_" + hex(sha256(user_id + "|" + subject + "|" + meaning + "|" + std::to_string(ts))).substr(0, 24);
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(id);
    d->obj["user_id"] = Json::mkstr(user_id);
    d->obj["subject"] = Json::mkstr(subject);
    d->obj["meaning"] = Json::mkstr(meaning);
    if (payload) d->obj["payload"] = payload;
    d->obj["signed_at"] = Json::mknum((double)ts);
    store_.put("signatures", id, d);
    store_.append("signatures_log", d);
    return id;
}

std::vector<JsonP> Identity::signatures(const std::string& subject) const {
    std::vector<JsonP> v;
    for (const auto& id : store_.list("signatures")) {
        JsonP s = store_.get("signatures", id);
        if (s && (subject.empty() || s->str("subject") == subject)) v.push_back(s);
    }
    return v;
}

}
