#pragma once
#include "merkle/merkle.hpp"
#include "proof/proof.hpp"
#include <string>

namespace fve {

struct RunResult {
    Verdict verdict;
    EvalResult eval;
    Certificate cert;
    Digest audit_leaf;
    JsonP report;
};

Program load_program(const std::string& json_text, const HoleTable& holes = {});

RunResult run(const Program& p, AuditChain& chain, const std::string& method = "interval");

JsonP make_report(const RunResult& r);

}
