#pragma once
#include "../enclosure/enclosure.hpp"
#include <utility>
#include <vector>

namespace fve {

std::pair<Box, Box> bisect(const Box& b);
double box_max_width(const Box& b);

struct SiviaResult {
    std::vector<Box> safe_boxes;     // guaranteed ms_lo - residual > 0
    std::vector<Box> unknown_boxes;  // undecided (too narrow / budget hit)
    int iterations = 0;
    double safe_volume_fraction = 0.0;
};

struct SiviaConfig {
    double epsilon = 0.01;
    int max_iterations = 100000;
    int n_threads = 1;
};

SiviaResult sivia(const Program& p, const Box& initial_box, const SiviaConfig& cfg = {});

}
