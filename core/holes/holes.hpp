#pragma once
#include "../interval/ival.hpp"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fve {

struct WrapResult {
    Ival encl;
    double residual = 0.0;
    bool refuted = false;
};

using Wrapper = std::function<WrapResult(const std::vector<Ival>&)>;

struct Probe {
    std::vector<double> x;
    double f;
};

WrapResult phys_wrap(const Ival& phys, const std::vector<Ival>& in);

Wrapper monotone_wrapper(int sign, Ival phys,
                         std::function<double(const std::vector<double>&)> f);

Wrapper lipschitz_wrapper(std::vector<Probe> probes, double l_use, Ival phys);

struct HoleTable {
    std::map<std::string, Wrapper> wrappers;
    std::map<std::string, Ival> phys;
    void set(const std::string& id, Wrapper w) { wrappers[id] = std::move(w); }
    void set_phys(const std::string& id, Ival p) { phys[id] = p; }
    bool has(const std::string& id) const { return wrappers.count(id) > 0; }
    WrapResult eval(const std::string& id, const Ival& fallback,
                    const std::vector<Ival>& in) const;
};

Ival bundle_hull(const std::vector<Ival>& cands);

}
