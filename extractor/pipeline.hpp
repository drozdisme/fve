#pragma once
#include "../core/engine.hpp"
#include "../model/lower/lower.hpp"
#include "../model/sdg/sdg.hpp"
#include "../oracle/intervene.hpp"
#include <string>

namespace fve {

struct Extracted {
    Workbook wb;
    DepGraph dep;
    Sdg sdg;
    std::string fmt;
    bool ok = false;
    std::string err;
};

Extracted extract_file(const std::string& path);

struct Verified {
    CellId target;
    std::string label;
    Lowered lowered;
    RunResult run;
    bool ok = false;
};

Verified verify_target(Workbook& wb, Oracle& orc, const Sdg& sdg, const CellId& target,
                       double rel_dev, AuditChain& chain);

std::vector<CellId> margin_targets(const Sdg& sdg);

}
