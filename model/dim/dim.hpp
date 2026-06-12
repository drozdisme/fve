#pragma once
#include "../../core/types/types.hpp"
#include <map>
#include <string>
#include <vector>

namespace fve {

using IMat = std::vector<std::vector<long long>>;

struct Smith {
    IMat u;
    IMat d;
    IMat v;
    int rank = 0;
};

Smith smith_normal_form(IMat a);

struct DimConstraint {
    std::vector<std::pair<int, long long>> terms;
    Dim rhs;
};

struct DimGraph {
    std::vector<std::string> syms;
    std::map<std::string, int> idx;
    std::vector<DimConstraint> cons;
    std::map<int, Dim> known;

    int sym(const std::string& name);
    void add_eq(int a, int b);
    void add_combo(int out, const std::vector<std::pair<int, long long>>& in);
    void set_known(int s, const Dim& d);
};

struct DimResult {
    std::map<std::string, Dim> dims;
    bool consistent = true;
    std::vector<std::string> clashes;
};

DimResult infer_dims(const DimGraph& g);

}
