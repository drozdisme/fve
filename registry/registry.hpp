#pragma once
#include "../db/store.hpp"
#include "../core/merkle/sha256.hpp"
#include <string>
#include <vector>

namespace fve {

struct Entry {
    std::string id;
    std::string kind;
    int version = 1;
    std::string hash;
    JsonP meta;
};

class Registry {
public:
    explicit Registry(Store& s) : store_(s) {}

    std::string put_artifact(const std::string& name, const std::string& mime,
                             const std::vector<uint8_t>& bytes, const JsonP& meta);
    std::string put_model(const std::string& artifact_id, const JsonP& model, const JsonP& meta);
    int version_of(const std::string& kind, const std::string& key) const;

    JsonP get(const std::string& kind, const std::string& id) const;
    std::vector<std::string> list(const std::string& kind) const;
    void link(const std::string& from, const std::string& to, const std::string& rel);
    std::vector<JsonP> provenance(const std::string& id) const;

private:
    Store& store_;
};

}
