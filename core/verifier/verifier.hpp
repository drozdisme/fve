#pragma once
#include "../enclosure/enclosure.hpp"
#include <string>

namespace fve {

enum class Verdict { Safe, Fail, Abstain };

std::string verdict_name(Verdict v);

Verdict decide(double ms_lo, double ms_hi, double residual);

Verdict decide_eval(const EvalResult& r);

}
