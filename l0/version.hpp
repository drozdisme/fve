#pragma once
#include <string>

namespace fve {

struct Rev {
    std::string config_key;
    std::string label;
    double rank = 0.0;
    bool found = false;
};

Rev parse_revision(const std::string& filename);

}
