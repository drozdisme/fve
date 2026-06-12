#pragma once
#include "../../core/ast/ast.hpp"
#include <string>
#include <vector>

namespace fve {

struct Candidate {
    Nodep node;
    double conf = 1.0;
    std::string prov;
};

struct Bundle {
    std::vector<Candidate> cands;
    bool unknown = false;
    Ival unknown_phys = Ival::entire();

    void add(Nodep n, double conf, const std::string& prov);
    void mark_unknown(Ival phys);
    void normalize();
    void prune(double floor);
    size_t size() const { return cands.size(); }
    bool ambiguous() const { return cands.size() > 1 || unknown; }
    double top_conf() const;
    Nodep lower(const std::string& hole_id) const;
};

}
