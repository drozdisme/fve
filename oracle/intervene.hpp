#pragma once
#include "oracle.hpp"
#include <map>
#include <vector>

namespace fve {

struct DepEdge {
    CellId from;
    CellId to;
    double sens;
};

struct DepGraph {
    std::vector<CellId> inputs;
    std::vector<CellId> outputs;
    std::vector<DepEdge> edges;
    std::map<CellId, std::vector<CellId>> deps_of;
};

DepGraph induce_deps(Oracle& orc, double rel = 1e-3, double tol = 1e-7);

}
