#include "bundle.hpp"
#include <algorithm>

namespace fve {

void Bundle::add(Nodep n, double conf, const std::string& prov) {
    cands.push_back({n, conf, prov});
}

void Bundle::mark_unknown(Ival phys) {
    unknown = true;
    unknown_phys = meet(unknown_phys.is_entire() ? phys : unknown_phys, phys);
    if (unknown_phys.is_empty()) unknown_phys = phys;
}

void Bundle::normalize() {
    double s = 0.0;
    for (auto& c : cands) s += c.conf;
    if (s > 0.0)
        for (auto& c : cands) c.conf /= s;
}

void Bundle::prune(double floor) {
    if (cands.size() <= 1) return;
    std::sort(cands.begin(), cands.end(),
              [](const Candidate& a, const Candidate& b) { return a.conf > b.conf; });
    std::vector<Candidate> keep;
    for (const auto& c : cands)
        if (c.conf >= floor) keep.push_back(c);
    if (keep.size() < 2) {
        keep.clear();
        keep.push_back(cands[0]);
        if (cands.size() > 1) keep.push_back(cands[1]);
    }
    cands = keep;
}

double Bundle::top_conf() const {
    double m = 0.0;
    for (const auto& c : cands) m = std::max(m, c.conf);
    return m;
}

Nodep Bundle::lower(const std::string& hole_id) const {
    std::vector<Nodep> parts;
    for (const auto& c : cands)
        if (c.node) parts.push_back(c.node);
    if (unknown || parts.empty())
        parts.push_back(hole(hole_id, 0, unknown_phys, 'B', {}));
    if (parts.size() == 1) return parts[0];
    return disj(parts);
}

}
