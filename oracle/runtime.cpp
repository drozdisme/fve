#include "runtime.hpp"
#include <cmath>

namespace fve {

Runtime::Runtime(Workbook& wb, RunLimits lim) : wb_(wb), orc_(wb), lim_(lim) {}

bool Runtime::run(RunStats& stats) {
    orc_.build();
    stats.cells = 0;
    stats.evals = 0;
    for (auto& s : wb_.sheets) stats.cells += (int)s.cells.size();
    if (stats.cells > lim_.max_cells) {
        stats.limit_hit = true;
        return false;
    }
    artifact_ = Json::mkobj();
    auto outs = Json::mkobj();
    for (const auto& o : orc_.outputs()) {
        double v = orc_.value(o.sheet, o.row, o.col);
        stats.evals++;
        outs->obj[wb_.key(o.sheet, o.row, o.col)] = Json::mknum(v);
        if (stats.evals > lim_.max_evals) {
            stats.limit_hit = true;
            return false;
        }
    }
    auto ins = Json::mkobj();
    for (const auto& i : orc_.inputs())
        ins->obj[wb_.key(i.sheet, i.row, i.col)] = Json::mknum(orc_.value(i.sheet, i.row, i.col));
    artifact_->obj["inputs"] = ins;
    artifact_->obj["outputs"] = outs;
    artifact_->obj["cells"] = Json::mknum(stats.cells);
    artifact_->obj["evals"] = Json::mknum(stats.evals);
    return true;
}

JsonP Runtime::capture() const { return artifact_ ? artifact_ : Json::mkobj(); }

}
