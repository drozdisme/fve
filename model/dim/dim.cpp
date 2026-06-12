#include "dim.hpp"
#include <cstdlib>

namespace fve {

namespace {

IMat ident(int n) {
    IMat m(n, std::vector<long long>(n, 0));
    for (int i = 0; i < n; i++) m[i][i] = 1;
    return m;
}

void row_swap(IMat& a, int r1, int r2) { std::swap(a[r1], a[r2]); }

void col_swap(IMat& a, int c1, int c2) {
    for (auto& row : a) std::swap(row[c1], row[c2]);
}

void row_addmul(IMat& a, int dst, int src, long long q) {
    if (q == 0) return;
    for (size_t j = 0; j < a[dst].size(); j++) a[dst][j] -= q * a[src][j];
}

void col_addmul(IMat& a, int dst, int src, long long q) {
    if (q == 0) return;
    for (size_t i = 0; i < a.size(); i++) a[i][dst] -= q * a[i][src];
}

IMat matmul(const IMat& a, const IMat& b) {
    int m = a.size(), k = b.size(), n = b.empty() ? 0 : b[0].size();
    IMat c(m, std::vector<long long>(n, 0));
    for (int i = 0; i < m; i++)
        for (int t = 0; t < k; t++)
            if (a[i][t])
                for (int j = 0; j < n; j++) c[i][j] += a[i][t] * b[t][j];
    return c;
}

}

Smith smith_normal_form(IMat a) {
    int m = a.size();
    int n = m ? a[0].size() : 0;
    IMat u = ident(m), v = ident(n);
    int t = 0;
    int lim = m < n ? m : n;
    while (t < lim) {
        int pi = -1, pj = -1;
        long long best = 0;
        for (int i = t; i < m; i++)
            for (int j = t; j < n; j++)
                if (a[i][j] != 0) {
                    long long av = std::llabs(a[i][j]);
                    if (pi < 0 || av < best) { best = av; pi = i; pj = j; }
                }
        if (pi < 0) break;
        if (pi != t) { row_swap(a, t, pi); row_swap(u, t, pi); }
        if (pj != t) { col_swap(a, t, pj); col_swap(v, t, pj); }
        bool again = true;
        while (again) {
            again = false;
            for (int i = t + 1; i < m; i++)
                if (a[i][t] != 0) {
                    long long q = a[i][t] / a[t][t];
                    row_addmul(a, i, t, q);
                    row_addmul(u, i, t, q);
                    if (a[i][t] != 0) { row_swap(a, t, i); row_swap(u, t, i); again = true; }
                }
            for (int j = t + 1; j < n; j++)
                if (a[t][j] != 0) {
                    long long q = a[t][j] / a[t][t];
                    col_addmul(a, j, t, q);
                    col_addmul(v, j, t, q);
                    if (a[t][j] != 0) { col_swap(a, t, j); col_swap(v, t, j); again = true; }
                }
        }
        for (int i = t + 1; i < m && !again; i++)
            for (int j = t + 1; j < n; j++)
                if (a[i][j] % a[t][t] != 0) {
                    row_addmul(a, t, i, -1);
                    row_addmul(u, t, i, -1);
                    again = true;
                    break;
                }
        if (again) continue;
        t++;
    }
    Smith s;
    s.u = u;
    s.v = v;
    s.d = a;
    s.rank = 0;
    for (int i = 0; i < lim; i++) {
        if (s.d[i][i] < 0) {
            for (int j = 0; j < n; j++) s.d[i][j] = -s.d[i][j];
            for (int j = 0; j < m; j++) s.u[i][j] = -s.u[i][j];
        }
        if (s.d[i][i] != 0) s.rank++;
    }
    return s;
}

int DimGraph::sym(const std::string& name) {
    auto it = idx.find(name);
    if (it != idx.end()) return it->second;
    int i = syms.size();
    idx[name] = i;
    syms.push_back(name);
    return i;
}

void DimGraph::add_eq(int a, int b) { cons.push_back({{{a, 1}, {b, -1}}, Dim::none()}); }

void DimGraph::add_combo(int out, const std::vector<std::pair<int, long long>>& in) {
    DimConstraint c;
    c.terms.push_back({out, 1});
    for (auto& p : in) c.terms.push_back({p.first, -p.second});
    cons.push_back(c);
}

void DimGraph::set_known(int s, const Dim& d) { known[s] = d; }

DimResult infer_dims(const DimGraph& g) {
    DimResult res;
    int n = g.syms.size();
    int m = g.cons.size();
    std::vector<int> unk;
    std::vector<int> umap(n, -1);
    for (int s = 0; s < n; s++)
        if (!g.known.count(s)) { umap[s] = unk.size(); unk.push_back(s); }
    int nu = unk.size();

    IMat a(m, std::vector<long long>(nu, 0));
    IMat b(m, std::vector<long long>(7, 0));
    for (int r = 0; r < m; r++) {
        for (auto& term : g.cons[r].terms) {
            int s = term.first;
            long long co = term.second;
            if (umap[s] >= 0) {
                a[r][umap[s]] += co;
            } else {
                Dim d = g.known.at(s);
                for (int k = 0; k < 7; k++) b[r][k] -= co * d.e[k];
            }
        }
        for (int k = 0; k < 7; k++) b[r][k] -= g.cons[r].rhs.e[k];
    }

    Smith s = smith_normal_form(a);
    IMat ub = matmul(s.u, b);
    int lim = m < nu ? m : nu;

    IMat y(nu, std::vector<long long>(7, 0));
    for (int k = 0; k < 7; k++) {
        for (int i = 0; i < lim; i++) {
            long long dii = s.d[i][i];
            if (dii != 0) {
                if (ub[i][k] % dii != 0) { res.consistent = false; }
                else y[i][k] = ub[i][k] / dii;
            } else {
                if (ub[i][k] != 0) res.consistent = false;
            }
        }
        for (int i = lim; i < m; i++)
            if (ub[i][k] != 0) res.consistent = false;
    }

    std::vector<bool> free_y(nu, false);
    for (int i = 0; i < nu; i++)
        if (i >= lim || s.d[i][i] == 0) free_y[i] = true;

    IMat x = matmul(s.v, y);
    std::vector<bool> determined(nu, true);
    for (int j = 0; j < nu; j++)
        for (int i = 0; i < nu; i++)
            if (free_y[i] && s.v[j][i] != 0) determined[j] = false;

    for (auto& kv : g.known) res.dims[g.syms[kv.first]] = kv.second;
    for (int j = 0; j < nu; j++) {
        if (!determined[j]) continue;
        Dim d;
        for (int k = 0; k < 7; k++) d.e[k] = (int)x[j][k];
        res.dims[g.syms[unk[j]]] = d;
    }

    if (!res.consistent) {
        for (int r = 0; r < m; r++) {
            for (auto& term : g.cons[r].terms) {
                int s2 = term.first;
                res.clashes.push_back(g.syms[s2]);
            }
        }
    }
    return res;
}

}
