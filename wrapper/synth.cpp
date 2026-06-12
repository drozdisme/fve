#include "synth.hpp"
#include <cmath>

namespace fve {

namespace {

void grid(const std::vector<Ival>& dom, int per, std::vector<std::vector<double>>& pts) {
    if (dom.empty()) {
        pts.push_back({});
        return;
    }
    std::vector<std::vector<double>> rest;
    std::vector<Ival> tail(dom.begin() + 1, dom.end());
    grid(tail, per, rest);
    const Ival& d = dom[0];
    for (int i = 0; i < per; i++) {
        double t = per == 1 ? 0.5 : (double)i / (per - 1);
        double x = d.lo + t * (d.hi - d.lo);
        for (const auto& r : rest) {
            std::vector<double> p;
            p.push_back(x);
            p.insert(p.end(), r.begin(), r.end());
            pts.push_back(p);
        }
    }
}

}

LocalModel probe_model(const ProbeFn& fn, int per_dim) {
    LocalModel m;
    std::vector<std::vector<double>> pts;
    grid(fn.domain, per_dim, pts);
    for (const auto& p : pts) {
        double v = fn.f(p);
        m.probes.push_back({p, v});
        m.range = hull(m.range, Ival::point(v));
    }
    double l = 0.0;
    for (size_t i = 0; i < m.probes.size(); i++)
        for (size_t j = i + 1; j < m.probes.size(); j++) {
            double d2 = 0.0;
            for (size_t k = 0; k < m.probes[i].x.size(); k++) {
                double dk = m.probes[i].x[k] - m.probes[j].x[k];
                d2 += dk * dk;
            }
            double dist = std::sqrt(d2);
            if (dist > 1e-12) {
                double slope = std::fabs(m.probes[i].f - m.probes[j].f) / dist;
                if (slope > l) l = slope;
            }
        }
    m.lipschitz = l;
    if (fn.domain.size() == 1 && m.probes.size() >= 2) {
        bool inc = true, dec = true;
        for (size_t i = 1; i < m.probes.size(); i++) {
            if (m.probes[i].f < m.probes[i - 1].f - 1e-12) inc = false;
            if (m.probes[i].f > m.probes[i - 1].f + 1e-12) dec = false;
        }
        if (inc) { m.monotone = true; m.sign = 1; }
        else if (dec) { m.monotone = true; m.sign = -1; }
    }
    return m;
}

LocalModel probe_adaptive(const ProbeFn& fn, int per_dim, int rounds) {
    LocalModel m = probe_model(fn, per_dim);
    for (int r = 0; r < rounds; r++) {
        if (m.probes.size() < 2) break;
        size_t bi = 0, bj = 1;
        double best = -1.0;
        for (size_t i = 0; i < m.probes.size(); i++)
            for (size_t j = i + 1; j < m.probes.size(); j++) {
                double d2 = 0.0;
                for (size_t k = 0; k < m.probes[i].x.size(); k++) {
                    double dk = m.probes[i].x[k] - m.probes[j].x[k];
                    d2 += dk * dk;
                }
                double dist = std::sqrt(d2);
                if (dist <= 1e-12) continue;
                double slope = std::fabs(m.probes[i].f - m.probes[j].f) / dist;
                if (slope > best) { best = slope; bi = i; bj = j; }
            }
        std::vector<double> mid(m.probes[bi].x.size());
        for (size_t k = 0; k < mid.size(); k++) mid[k] = 0.5 * (m.probes[bi].x[k] + m.probes[bj].x[k]);
        double fv = fn.f(mid);
        m.probes.push_back({mid, fv});
        m.range = hull(m.range, Ival::point(fv));
        double l = m.lipschitz;
        for (size_t i = 0; i + 1 == m.probes.size(); i++) {
            for (size_t j = 0; j < m.probes.size() - 1; j++) {
                double d2 = 0.0;
                for (size_t k = 0; k < mid.size(); k++) {
                    double dk = mid[k] - m.probes[j].x[k];
                    d2 += dk * dk;
                }
                double dist = std::sqrt(d2);
                if (dist > 1e-12) {
                    double slope = std::fabs(fv - m.probes[j].f) / dist;
                    if (slope > l) l = slope;
                }
            }
        }
        m.lipschitz = l;
    }
    return m;
}

Wrapper synth_wrapper(const LocalModel& m, double safety) {
    double l = m.lipschitz * safety;
    Ival phys = m.range;
    if (!phys.is_empty()) {
        double pad = 0.5 * phys.width() + 1e-9;
        phys = Ival::of(phys.lo - pad, phys.hi + pad);
    } else {
        phys = Ival::entire();
    }
    return lipschitz_wrapper(m.probes, l, phys);
}

Ival wrap_enclose(const LocalModel& m, const std::vector<Ival>& at, double safety) {
    Wrapper w = synth_wrapper(m, safety);
    WrapResult r = w(at);
    return r.encl;
}

}
