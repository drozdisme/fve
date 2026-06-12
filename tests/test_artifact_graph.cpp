#include "../platform/artifact_graph.hpp"
#include "../db/store.hpp"
#include "framework.hpp"
#include <algorithm>
#include <chrono>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string agtd() {
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_ag_" + std::to_string(getpid()) + "_" + std::to_string(ns);
}
bool has(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}
int idx(const std::vector<std::string>& v, const std::string& x) {
    return (int)(std::find(v.begin(), v.end(), x) - v.begin());
}
}

void test_artifact_graph() {
    cur = "artifact_graph";
    Store s(agtd());
    // stringer imports_from frame; assembly imports_from stringer
    add_artifact_dep(s, {"stringer", "frame", "F_load", "[frame.xlsx]S!A1"});
    add_artifact_dep(s, {"assembly", "stringer", "MS", "[stringer.xlsx]S!B2"});

    ArtifactGraph g = build_artifact_graph(s, "assembly", 3);
    CHECK(!g.has_cycle);
    CHECK(g.edges.size() == 2);
    CHECK(has(g.artifacts, "assembly") && has(g.artifacts, "stringer") && has(g.artifacts, "frame"));
    // topo: dependencies before dependents (frame before stringer before assembly)
    CHECK(idx(g.artifacts, "frame") < idx(g.artifacts, "stringer"));
    CHECK(idx(g.artifacts, "stringer") < idx(g.artifacts, "assembly"));

    // changing frame affects stringer and assembly
    auto aff = affected_artifacts(g, "frame");
    CHECK(has(aff, "stringer") && has(aff, "assembly"));
    // changing assembly affects nobody
    CHECK(affected_artifacts(g, "assembly").empty());

    // depth bound: from assembly with max_depth=1 reaches stringer but not frame
    ArtifactGraph g1 = build_artifact_graph(s, "assembly", 1);
    CHECK(has(g1.artifacts, "stringer"));
    CHECK(!has(g1.artifacts, "frame"));

    // cycle detection
    Store s2(agtd());
    add_artifact_dep(s2, {"a", "b", "x", "r1"});
    add_artifact_dep(s2, {"b", "a", "y", "r2"});
    ArtifactGraph gc = build_artifact_graph(s2, "a", 5);
    CHECK(gc.has_cycle);
}
