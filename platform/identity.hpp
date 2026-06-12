#pragma once
#include "../db/store.hpp"
#include <string>
#include <vector>

namespace fve {

struct User {
    std::string id;
    std::string external_id;
    std::string display_name;
    std::string role; // engineer, production_operator, reviewer, admin
    std::vector<std::string> section_scope;
    bool active = true;
    std::string api_token_hash;
};

class Identity {
public:
    explicit Identity(Store& s) : store_(s) {}

    std::string create_user(const User& u);
    JsonP get_user(const std::string& id) const;
    std::vector<JsonP> list_users() const;
    bool has_role(const std::string& user_id, const std::string& role) const;
    bool active(const std::string& user_id) const;

    // append-only electronic signature; returns signature id
    std::string sign(const std::string& user_id, const std::string& subject,
                     const std::string& meaning, const JsonP& payload);
    std::vector<JsonP> signatures(const std::string& subject) const;

private:
    Store& store_;
};

}
