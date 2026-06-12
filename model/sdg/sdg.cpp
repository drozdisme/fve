#include "sdg.hpp"
#include "../../extractor/formula/formula.hpp"
#include <algorithm>

namespace fve {

int Sdg::node(const CellId& id) {
    auto it = node_idx.find(id);
    if (it != node_idx.end()) return it->second;
    int i = nodes.size();
    node_idx[id] = i;
    SdgNode n;
    n.id = id;
    auto lit = labels.find(id);
    if (lit != labels.end()) n.label = lit->second;
    nodes.push_back(n);
    return i;
}

const SdgNode* Sdg::find(const CellId& id) const {
    auto it = node_idx.find(id);
    return it == node_idx.end() ? nullptr : &nodes[it->second];
}

namespace {

void resolve_labels(Workbook& wb, Sdg& g) {
    for (size_t si = 0; si < wb.sheets.size(); si++) {
        const Sheet& sh = wb.sheets[si];
        for (auto& kv : sh.cells) {
            int r = kv.first.first, c = kv.first.second;
            const Cell& cell = kv.second;
            if (cell.has_value && !cell.text.empty()) {
                const Cell* right = sh.at(r, c + 1);
                if (right && (right->has_value || !right->formula.empty()))
                    g.labels[{(int)si, r, c + 1}] = cell.text;
            }
        }
    }
    for (auto& kv : wb.names) {
        Ref t = kv.second.target;
        if (t.sheet < 0) t.sheet = 0;
        g.labels[{t.sheet, t.row, t.col}] = kv.first;
    }
}

void walk_refs(Workbook& wb, const XAstp& a, int cur, const CellId& to, Sdg& g) {
    if (!a) return;
    auto push = [&](int sh2, int r, int c) {
        if (sh2 < 0) sh2 = cur;
        g.edges.push_back({{sh2, r, c}, to, Origin::Syntactic, 0.0});
        g.node({sh2, r, c});
    };
    if (a->kind == XKind::Cell) {
        int sh = a->sheet.empty() ? cur : wb.sheet_index(a->sheet);
        push(sh, a->ref.row, a->ref.col);
    } else if (a->kind == XKind::Range) {
        int sh = a->sheet.empty() ? cur : wb.sheet_index(a->sheet);
        int r0 = std::min(a->ref.row, a->ref_end.row), r1 = std::max(a->ref.row, a->ref_end.row);
        int c0 = std::min(a->ref.col, a->ref_end.col), c1 = std::max(a->ref.col, a->ref_end.col);
        for (int r = r0; r <= r1; r++)
            for (int c = c0; c <= c1; c++) push(sh, r, c);
    } else if (a->kind == XKind::NameRef) {
        auto it = wb.names.find(a->name);
        if (it != wb.names.end()) {
            Ref t = it->second.target;
            push(t.sheet, t.row, t.col);
        }
    }
    for (const auto& k : a->args) walk_refs(wb, k, cur, to, g);
}

void add_syntactic(Workbook& wb, Sdg& g) {
    for (size_t si = 0; si < wb.sheets.size(); si++) {
        Sheet& sh = wb.sheets[si];
        for (auto& kv : sh.cells) {
            const Cell& cell = kv.second;
            if (cell.formula.empty()) continue;
            CellId to{(int)si, kv.first.first, kv.first.second};
            XParse p = parse_formula(cell.formula);
            if (!p.ok) continue;
            walk_refs(wb, p.ast, (int)si, to, g);
            g.node(to);
        }
    }
}

}

Sdg build_sdg(Workbook& wb, const DepGraph& dep) {
    Sdg g;
    resolve_labels(wb, g);
    for (const auto& in : dep.inputs) {
        int i = g.node(in);
        g.nodes[i].is_input = true;
    }
    for (const auto& o : dep.outputs) g.node(o);

    add_syntactic(wb, g);

    std::map<std::pair<long long, long long>, size_t> seen;
    auto key = [](const CellId& a) { return ((long long)a.sheet << 40) | ((long long)a.row << 20) | a.col; };
    for (size_t i = 0; i < g.edges.size(); i++)
        seen[{key(g.edges[i].from), key(g.edges[i].to)}] = i;

    for (const auto& e : dep.edges) {
        auto k = std::make_pair(key(e.from), key(e.to));
        auto it = seen.find(k);
        if (it != seen.end()) {
            g.edges[it->second].origin = Origin::Both;
            g.edges[it->second].sens = e.sens;
        } else {
            g.edges.push_back({e.from, e.to, Origin::Interventional, e.sens});
            g.node(e.from);
            g.node(e.to);
        }
    }
    for (auto& n : g.nodes) {
        auto lit = g.labels.find(n.id);
        if (lit != g.labels.end()) n.label = lit->second;
    }
    return g;
}

void mark_ambiguous(Sdg& g, const CellId& id, int alts) {
    int i = g.node(id);
    g.nodes[i].ambiguous = true;
    g.nodes[i].alts = alts;
}

}
