#pragma once
#include "../enclosure/enclosure.hpp"
#include "../merkle/sha256.hpp"
#include <map>
#include <string>
#include <vector>

namespace fve {

struct CritConfig {
    double alpha = 3.0;
    std::map<std::string, double> thresholds = {{"P1", 0.2}, {"P2", 0.5}, {"P3", 0.75}, {"P4", 0.9}};
    std::string schema_version = "1";
    std::string canonical() const;
    Digest hash() const;
};

struct CritScore {
    double cs = 0.0;     // [0,1]
    double snr = 0.0;    // signal-to-noise ratio
    int priority = 0;    // 0..4
    std::string label;   // SAFE,WATCH,REVIEW,CRITICAL,HALT
    std::string why;
};

CritScore criticality(const EvalResult& r, double alpha = 3.0);
CritScore criticality(const EvalResult& r, const CritConfig& cfg);

struct CritReduction {
    std::string hole_id;
    double width_contribution; // share of total trace width [0,1]
    std::string suggestion;
};

std::vector<CritReduction> reduction_plan(const EvalResult& r);

struct AbstainReason {
    std::string node_id;     // hex of node digest
    std::string node_label;
    std::string kind;        // hole_phys, hole_refuted, disjunction, wide_input
    double width_share;      // [0,1]
    std::string instruction;
};

struct AbstainReport {
    bool is_abstain = false;
    std::vector<AbstainReason> reasons; // sorted by width_share desc
    std::string summary;
};

AbstainReport explain_abstain(const EvalResult& r);

}
