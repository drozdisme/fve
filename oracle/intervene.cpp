#include "intervene.hpp"
#include <cmath>

namespace fve {

DepGraph induce_deps(Oracle& orc, double rel, double tol) {
    DepGraph g;
    g.inputs = orc.inputs();
    g.outputs = orc.outputs();

    orc.reset();
    std::map<CellId, double> base;
    for (const auto& o : g.outputs)
        base[o] = orc.value(o.sheet, o.row, o.col);

    for (const auto& in : g.inputs) {
        orc.reset();
        double b = orc.value(in.sheet, in.row, in.col);
        double d = std::fabs(b) > 1.0 ? std::fabs(b) * rel : rel;
        orc.set(in.sheet, in.row, in.col, b + d);
        for (const auto& o : g.outputs) {
            double v = orc.value(o.sheet, o.row, o.col);
            double resp = std::fabs(v - base[o]);
            if (!std::isnan(v) && resp > tol) {
                double sens = (v - base[o]) / d;
                g.edges.push_back({in, o, sens});
                g.deps_of[o].push_back(in);
            }
        }
    }
    orc.reset();
    return g;
}

}
