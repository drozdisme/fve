#pragma once
#include "../../oracle/intervene.hpp"
#include "../../model/workbook/workbook.hpp"
#include <map>
#include <string>
#include <vector>

namespace fve {

enum class Origin { Syntactic, Interventional, Both };

struct SdgNode {
    CellId id;
    std::string label;
    bool is_input = false;
    bool ambiguous = false;
    int alts = 1;
};

struct SdgEdge {
    CellId from;
    CellId to;
    Origin origin;
    double sens = 0.0;
};

struct Sdg {
    std::vector<SdgNode> nodes;
    std::vector<SdgEdge> edges;
    std::map<CellId, int> node_idx;
    std::map<CellId, std::string> labels;

    int node(const CellId& id);
    const SdgNode* find(const CellId& id) const;
};

Sdg build_sdg(Workbook& wb, const DepGraph& dep);
void mark_ambiguous(Sdg& g, const CellId& id, int alts);

}
