#pragma once
#include "../affine/aff.hpp"
#include "../ast/ast.hpp"
#include "../holes/holes.hpp"
#include <map>
#include <string>
#include <vector>

namespace fve {

using Box = std::map<std::string, Ival>;

struct Trace {
    Digest node;
    std::string label;
    Ival encl;
    double width;
    bool holed;
    char regime;
};

struct EvalResult {
    Ival ms;
    double residual;
    std::vector<Trace> trace;
};

struct Program {
    Nodep root;
    Box box;
    HoleTable holes;
    double ks = 0.0;
};

Ival eval_ival(const Nodep& n, const Box& box, const HoleTable& holes,
               double& residual);

EvalResult eval(const Program& p);

Ival eval_aff(const Nodep& n, const std::map<std::string, Aff>& env,
              const HoleTable& holes, double& residual);

double total_width(const std::vector<Trace>& t);

}
