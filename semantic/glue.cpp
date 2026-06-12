#include "glue.hpp"
#include <cmath>

namespace fve {

std::vector<std::pair<std::string, std::string>> match_sections(const Section& a, const Section& b) {
    std::vector<std::pair<std::string, std::string>> m;
    for (const auto& kv : a.sym)
        if (b.sym.count(kv.first)) m.push_back({a.region + ":" + kv.first, b.region + ":" + kv.first});
    return m;
}

namespace {

Ival restrict(const Ival& x, const Ival& y) { return meet(x, y); }

}

GlueResult glue(const std::vector<Section>& sections) {
    GlueResult r;
    std::map<std::string, std::pair<Assign, std::string>> acc;

    for (const auto& s : sections) {
        for (const auto& kv : s.sym) {
            const std::string& name = kv.first;
            const Assign& a = kv.second;
            auto it = acc.find(name);
            if (it == acc.end()) {
                acc[name] = {a, s.region};
                continue;
            }
            r.matches.push_back({it->second.second + ":" + name, s.region + ":" + name});
            Assign& cur = it->second.first;
            if (a.has_dim && cur.has_dim && a.dim != cur.dim) {
                r.conflicts.push_back({name, it->second.second, s.region, "dimension"});
                r.consistent = false;
                continue;
            }
            if (!cur.has_dim && a.has_dim) {
                cur.dim = a.dim;
                cur.has_dim = true;
            }
            Ival m = restrict(cur.range, a.range);
            if (m.is_empty()) {
                r.conflicts.push_back({name, it->second.second, s.region, "range"});
                r.consistent = false;
            } else {
                cur.range = m;
            }
        }
    }

    r.global.region = "glued";
    for (auto& kv : acc) r.global.sym[kv.first] = kv.second.first;
    r.obstructed = !r.consistent;
    return r;
}

namespace {

struct DSU {
    std::map<std::string, std::string> p;
    std::map<std::string, double> pot;
    std::string find(const std::string& x) {
        if (!p.count(x)) { p[x] = x; pot[x] = 0.0; }
        if (p[x] == x) return x;
        std::string r = find(p[x]);
        pot[x] += pot[p[x]];
        p[x] = r;
        return r;
    }
    double potential(const std::string& x) { find(x); return pot[x]; }
};

}

Cohomology cohomology(const std::vector<std::string>& regions, const std::vector<Overlap>& overlaps) {
    Cohomology h;
    std::map<std::string, std::vector<Overlap>> by_sym;
    for (const auto& o : overlaps) by_sym[o.symbol].push_back(o);

    std::map<std::string, int> comp_seed;
    for (const auto& reg : regions) comp_seed[reg] = 0;

    int total_nodes = 0, total_edges = 0, total_comp = 0;
    std::map<std::string, bool> seen_node;

    for (auto& kv : by_sym) {
        const std::string& sym = kv.first;
        DSU dsu;
        std::map<std::string, bool> nodes;
        std::vector<Overlap> tree_edges;
        std::vector<Overlap> back_edges;
        for (const auto& o : kv.second) {
            nodes[o.a] = true;
            nodes[o.b] = true;
            std::string ra = dsu.find(o.a), rb = dsu.find(o.b);
            if (ra != rb) {
                double pa = dsu.potential(o.a), pb = dsu.potential(o.b);
                dsu.p[rb] = ra;
                dsu.pot[rb] = pa + o.offset - pb;
                tree_edges.push_back(o);
            } else {
                back_edges.push_back(o);
            }
        }
        for (const auto& o : back_edges) {
            double disc = o.offset - (dsu.potential(o.b) - dsu.potential(o.a));
            Cocycle c;
            c.symbol = sym;
            c.loop = {o.a, o.b};
            c.discrepancy = disc;
            if (std::fabs(disc) > 1e-9) {
                h.obstructed = true;
                h.classes.push_back(c);
            }
            h.h1_rank++;
        }
        (void)tree_edges;
        total_nodes += (int)nodes.size();
        total_edges += (int)kv.second.size();
    }
    (void)total_comp;
    (void)seen_node;
    (void)comp_seed;
    h.h0 = (int)regions.size();
    return h;
}

}
