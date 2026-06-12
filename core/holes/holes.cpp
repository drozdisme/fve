#include "holes.hpp"
#include <cmath>

namespace fve {

WrapResult phys_wrap(const Ival& phys, const std::vector<Ival>&) {
    WrapResult r;
    r.encl = phys;
    r.residual = phys.is_entire() ? Ival::entire().hi : 0.5 * phys.width();
    return r;
}

Wrapper monotone_wrapper(int sign, Ival phys,
                         std::function<double(const std::vector<double>&)> f) {
    return [sign, phys, f](const std::vector<Ival>& in) -> WrapResult {
        std::vector<double> a, b;
        for (const auto& iv : in) {
            if (iv.is_empty()) return phys_wrap(phys, in);
            a.push_back(sign >= 0 ? iv.lo : iv.hi);
            b.push_back(sign >= 0 ? iv.hi : iv.lo);
        }
        double lo = f(a), hi = f(b);
        WrapResult r;
        r.encl = meet(Ival::of(lo, hi), phys);
        r.residual = 0.0;
        return r;
    };
}

Wrapper lipschitz_wrapper(std::vector<Probe> probes, double l_use, Ival phys) {
    return [probes, l_use, phys](const std::vector<Ival>& in) -> WrapResult {
        WrapResult r;
        r.encl = phys;
        if (probes.empty()) return phys_wrap(phys, in);
        std::vector<double> c;
        double rad = 0.0;
        for (const auto& iv : in) {
            c.push_back(iv.mid());
            rad += 0.5 * iv.width() * 0.5 * iv.width();
        }
        Ival acc = Ival::entire();
        for (const auto& p : probes) {
            double d2 = 0.0;
            for (size_t k = 0; k < c.size() && k < p.x.size(); k++) {
                double dk = c[k] - p.x[k];
                d2 += dk * dk;
            }
            double dist = std::sqrt(d2) + std::sqrt(rad);
            Ival band = add(Ival::point(p.f), Ival::of(-l_use * dist, l_use * dist));
            acc = meet(acc, band);
        }
        r.encl = meet(acc, phys);
        r.residual = 0.0;
        for (size_t i = 0; i < probes.size(); i++)
            for (size_t j = i + 1; j < probes.size(); j++) {
                double d2 = 0.0;
                for (size_t k = 0; k < probes[i].x.size(); k++) {
                    double dk = probes[i].x[k] - probes[j].x[k];
                    d2 += dk * dk;
                }
                double dist = std::sqrt(d2);
                if (dist > 0 && std::fabs(probes[i].f - probes[j].f) / dist > l_use)
                    r.refuted = true;
            }
        if (r.refuted) {
            r.encl = phys;
            r.residual = phys.is_entire() ? Ival::entire().hi : 0.5 * phys.width();
        }
        return r;
    };
}

WrapResult HoleTable::eval(const std::string& id, const Ival& fallback,
                           const std::vector<Ival>& in) const {
    auto it = wrappers.find(id);
    Ival ph = fallback;
    auto pit = phys.find(id);
    if (pit != phys.end()) ph = pit->second;
    if (it == wrappers.end()) return phys_wrap(ph, in);
    WrapResult r = it->second(in);
    if (r.refuted || r.encl.is_empty()) return phys_wrap(ph, in);
    return r;
}

Ival bundle_hull(const std::vector<Ival>& cands) {
    Ival r = Ival::empty();
    for (const auto& c : cands) r = hull(r, c);
    return r;
}

}
