#include "artifact_graph.hpp"
#include "../core/merkle/sha256.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <functional>

namespace fve {

void add_artifact_dep(Store& store, const ArtifactEdge& e) {
    auto d = Json::mkobj();
    d->obj["from"] = Json::mkstr(e.from_artifact);
    d->obj["to"] = Json::mkstr(e.to_artifact);
    d->obj["symbol"] = Json::mkstr(e.symbol);
    d->obj["cell_ref"] = Json::mkstr(e.cell_ref);
    std::string id = hex(sha256(e.from_artifact + "|" + e.to_artifact + "|" + e.symbol)).substr(0, 24);
    store.put("artifact_deps", id, d);
}

namespace {

std::vector<ArtifactEdge> all_edges(Store& store) {
    std::vector<ArtifactEdge> es;
    for (const auto& id : store.list("artifact_deps")) {
        JsonP d = store.get("artifact_deps", id);
        if (!d) continue;
        es.push_back({d->str("from"), d->str("to"), d->str("symbol"), d->str("cell_ref")});
    }
    return es;
}

}

ArtifactGraph build_artifact_graph(Store& store, const std::string& root_artifact, int max_depth) {
    ArtifactGraph g;
    auto edges = all_edges(store);
    std::map<std::string, std::vector<ArtifactEdge>> out;
    for (const auto& e : edges) out[e.from_artifact].push_back(e);

    // BFS from root bounded by depth, collecting reachable nodes + edges
    std::set<std::string> nodes;
    std::vector<std::pair<std::string, int>> q{{root_artifact, 0}};
    nodes.insert(root_artifact);
    while (!q.empty()) {
        auto cur = q.back();
        q.pop_back();
        if (cur.second >= max_depth) continue;
        for (const auto& e : out[cur.first]) {
            g.edges.push_back(e);
            if (!nodes.count(e.to_artifact)) {
                nodes.insert(e.to_artifact);
                q.push_back({e.to_artifact, cur.second + 1});
            }
        }
    }

    // topological sort (deps first) via DFS with cycle detection
    std::map<std::string, std::vector<std::string>> adj;
    for (const auto& e : g.edges) adj[e.from_artifact].push_back(e.to_artifact);
    std::map<std::string, int> state; // 0 unvisited,1 in-progress,2 done
    std::vector<std::string> order;
    std::function<void(const std::string&)> dfs = [&](const std::string& u) {
        state[u] = 1;
        for (const auto& v : adj[u]) {
            if (state[v] == 1) { g.has_cycle = true; continue; }
            if (state[v] == 0) dfs(v);
        }
        state[u] = 2;
        order.push_back(u); // post-order => dependencies before dependents
    };
    for (const auto& n : nodes)
        if (state[n] == 0) dfs(n);
    g.artifacts = order; // deps first
    return g;
}

std::vector<std::string> affected_artifacts(const ArtifactGraph& g, const std::string& changed) {
    // reverse edges: who imports_from `changed` (directly or transitively)
    std::map<std::string, std::vector<std::string>> rev;
    for (const auto& e : g.edges) rev[e.to_artifact].push_back(e.from_artifact);
    std::set<std::string> seen;
    std::vector<std::string> stack{changed}, out;
    while (!stack.empty()) {
        std::string u = stack.back();
        stack.pop_back();
        for (const auto& dep : rev[u]) {
            if (seen.insert(dep).second) { out.push_back(dep); stack.push_back(dep); }
        }
    }
    return out;
}

}
