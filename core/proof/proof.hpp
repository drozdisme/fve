#pragma once
#include "../ast/json.hpp"
#include "../merkle/sha256.hpp"
#include "../verifier/verifier.hpp"
#include <string>
#include <vector>

namespace fve {

struct Certificate {
    Digest program_hash;
    double ms_lo = 0.0;
    double ms_hi = 0.0;
    double residual = 0.0;
    double ks = 0.0;
    std::string method;
    char regime = 'A';
    Verdict verdict = Verdict::Abstain;
    std::vector<std::string> conditions;
    Digest digest;
};

Certificate build_cert(const Program& p, const EvalResult& r,
                       const std::string& method);

JsonP cert_json(const Certificate& c);
Certificate cert_from_json(const JsonP& j);

Digest cert_hash(const Certificate& c);
bool validate(const Certificate& c);

std::string cert_dump(const Certificate& c);

}
