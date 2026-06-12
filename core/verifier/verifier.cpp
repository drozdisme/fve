#include "verifier.hpp"
#include <cmath>

namespace fve {

std::string verdict_name(Verdict v) {
    switch (v) {
        case Verdict::Safe: return "SAFE";
        case Verdict::Fail: return "FAIL";
        default: return "ABSTAIN";
    }
}

Verdict decide(double ms_lo, double ms_hi, double residual) {
    if (std::isnan(ms_lo) || std::isnan(ms_hi)) return Verdict::Abstain;
    if (std::isinf(ms_lo) || std::isinf(ms_hi)) return Verdict::Abstain;
    if (residual < 0.0) return Verdict::Abstain;
    double lower = ms_lo - residual;
    double upper = ms_hi + residual;
    if (lower > 0.0) return Verdict::Safe;
    if (upper < 0.0) return Verdict::Fail;
    return Verdict::Abstain;
}

Verdict decide_eval(const EvalResult& r) {
    if (r.ms.is_empty() || r.ms.is_entire()) return Verdict::Abstain;
    return decide(r.ms.lo, r.ms.hi, r.residual);
}

}
