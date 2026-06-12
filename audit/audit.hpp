#pragma once
#include "../db/store.hpp"
#include "../core/merkle/merkle.hpp"
#include <string>
#include <vector>

namespace fve {

class Audit {
public:
    explicit Audit(Store& s);
    Digest record(const std::string& type, const std::string& subject, const JsonP& payload,
                  const std::string& user_id = "system");
    std::vector<JsonP> history(const std::string& subject = "") const;
    std::vector<JsonP> lineage(const std::string& artifact) const;
    std::vector<JsonP> certificates() const;
    bool verify() const;
    std::string head() const { return hex(chain_.head()); }

private:
    Store& store_;
    AuditChain chain_;
    void rebuild();
};

}
