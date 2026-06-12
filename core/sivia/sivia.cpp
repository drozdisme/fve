#include "sivia.hpp"
#include <cmath>

namespace fve {

double box_max_width(const Box& b) {
    double m = 0.0;
    for (const auto& kv : b) m = std::max(m, kv.second.width());
    return m;
}

std::pair<Box, Box> bisect(const Box& b) {
    std::string widest;
    double max_w = -1.0;
    for (const auto& kv : b)
        if (kv.second.width() > max_w) { max_w = kv.second.width(); widest = kv.first; }
    Box b1 = b, b2 = b;
    if (!widest.empty()) {
        double mid = b.at(widest).mid();
        b1[widest] = Ival::of(b.at(widest).lo, mid);
        b2[widest] = Ival::of(mid, b.at(widest).hi);
    }
    return {b1, b2};
}

namespace {

double box_volume(const Box& b) {
    double v = 1.0;
    for (const auto& kv : b) {
        double w = kv.second.width();
        v *= (w > 0 ? w : 1.0);
    }
    return v;
}

}

SiviaResult sivia(const Program& p, const Box& initial_box, const SiviaConfig& cfg) {
    SiviaResult res;
    std::vector<Box> work;
    work.push_back(initial_box);
    double total_vol = box_volume(initial_box);
    double safe_vol = 0.0;

    while (!work.empty() && res.iterations < cfg.max_iterations) {
        Box x = work.back();
        work.pop_back();
        res.iterations++;

        Program pp = p;
        pp.box = x;
        EvalResult r = eval(pp);

        if (std::isnan(r.ms.lo) || std::isnan(r.ms.hi)) {
            if (box_max_width(x) < cfg.epsilon) res.unknown_boxes.push_back(x);
            else { auto pr = bisect(x); work.push_back(pr.first); work.push_back(pr.second); }
            continue;
        }
        if (r.ms.lo - r.residual > 0.0) {
            res.safe_boxes.push_back(x);
            safe_vol += box_volume(x);
        } else if (r.ms.hi + r.residual < 0.0) {
            // guaranteed FAIL — discard
        } else if (box_max_width(x) < cfg.epsilon) {
            res.unknown_boxes.push_back(x);
        } else {
            auto pr = bisect(x);
            work.push_back(pr.first);
            work.push_back(pr.second);
        }
    }
    for (const auto& x : work) res.unknown_boxes.push_back(x);
    res.safe_volume_fraction = total_vol > 0 ? safe_vol / total_vol : 0.0;
    return res;
}

}
