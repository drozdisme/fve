#pragma once
#include "oracle.hpp"
#include "../core/ast/json.hpp"
#include <string>

namespace fve {

struct RunLimits {
    int max_cells = 100000;
    int max_evals = 2000000;
};

struct RunStats {
    int evals = 0;
    int cells = 0;
    bool limit_hit = false;
};

class Runtime {
public:
    Runtime(Workbook& wb, RunLimits lim = {});
    bool run(RunStats& stats);
    JsonP capture() const;
    Oracle& oracle() { return orc_; }

private:
    Workbook& wb_;
    Oracle orc_;
    RunLimits lim_;
    JsonP artifact_;
};

}
