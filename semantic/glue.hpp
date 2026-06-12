#pragma once
#include "../core/types/types.hpp"
#include <map>
#include <string>
#include <vector>

namespace fve {

struct Assign {
    Ival range = Ival::entire();
    Dim dim;
    bool has_dim = false;
};

struct Section {
    std::string region;
    std::map<std::string, Assign> sym;
};

struct Conflict {
    std::string symbol;
    std::string region_a;
    std::string region_b;
    std::string kind;
};

struct GlueResult {
    Section global;
    std::vector<Conflict> conflicts;
    std::vector<std::pair<std::string, std::string>> matches;
    bool consistent = true;
    bool obstructed = false;
};

std::vector<std::pair<std::string, std::string>> match_sections(const Section& a, const Section& b);

GlueResult glue(const std::vector<Section>& sections);

struct Overlap {
    std::string a;
    std::string b;
    std::string symbol;
    double offset = 0.0;
};

struct Cocycle {
    std::string symbol;
    std::vector<std::string> loop;
    double discrepancy = 0.0;
};

struct Cohomology {
    int h0 = 0;
    int h1_rank = 0;
    bool obstructed = false;
    std::vector<Cocycle> classes;
};

Cohomology cohomology(const std::vector<std::string>& regions, const std::vector<Overlap>& overlaps);

}
