#pragma once
#include "../core/interval/ival.hpp"
#include "../db/store.hpp"
#include <string>
#include <vector>

namespace fve {

enum class VerdictTransition { SafeSafe, SafeAbstain, AbstainSafe, ToFail, FromFail, Other };

std::string transition_name(VerdictTransition t);

struct TargetDelta {
    std::string cell;
    std::string label;
    Ival ms_old;
    Ival ms_new;
    double delta_lo = 0.0;
    double delta_hi = 0.0;
    double delta_width = 0.0;
    double cs_old = 0.0;
    double cs_new = 0.0;
    double delta_cs = 0.0;
    std::string verdict_old;
    std::string verdict_new;
    VerdictTransition transition = VerdictTransition::Other;
    int holes_old = 0;
    int holes_new = 0;
};

struct CertDiff {
    std::string run_old;
    std::string run_new;
    std::string artifact;
    std::vector<TargetDelta> targets; // sorted by |delta_cs| desc (hot first)
    std::vector<std::string> new_holes;
    std::vector<std::string> closed_holes;
    bool safe = true; // run_new not worse than run_old
    std::string summary;
    bool ok = false;
};

CertDiff diff_runs(Store& store, const std::string& run_a, const std::string& run_b);
JsonP cert_diff_json(const CertDiff& d);

}
