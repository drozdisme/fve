#pragma once
#include "../db/store.hpp"
#include <string>
#include <vector>

namespace fve {

struct ArtifactEdge {
    std::string from_artifact;
    std::string to_artifact; // from imports_from to
    std::string symbol;
    std::string cell_ref;
};

struct ArtifactGraph {
    std::vector<std::string> artifacts; // topologically sorted (deps first)
    std::vector<ArtifactEdge> edges;
    bool has_cycle = false;
};

void add_artifact_dep(Store& store, const ArtifactEdge& e);

// Build from stored artifact_deps, bounded by max_depth from root.
ArtifactGraph build_artifact_graph(Store& store, const std::string& root_artifact, int max_depth = 3);

// Artifacts that (transitively) depend on `changed` and must be re-verified.
std::vector<std::string> affected_artifacts(const ArtifactGraph& g, const std::string& changed);

}
