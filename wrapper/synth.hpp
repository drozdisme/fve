#pragma once
#include "../core/holes/holes.hpp"
#include "../oracle/oracle.hpp"
#include <functional>
#include <string>
#include <vector>

namespace fve {

struct ProbeFn {
    std::function<double(const std::vector<double>&)> f;
    std::vector<Ival> domain;
};

struct LocalModel {
    std::vector<Probe> probes;
    double lipschitz = 0.0;
    Ival range = Ival::empty();
    int sign = 0;
    bool monotone = false;
};

LocalModel probe_model(const ProbeFn& fn, int per_dim = 5);

LocalModel probe_adaptive(const ProbeFn& fn, int per_dim = 5, int rounds = 2);

Wrapper synth_wrapper(const LocalModel& m, double safety = 1.25);

Ival wrap_enclose(const LocalModel& m, const std::vector<Ival>& at, double safety = 1.25);

}
