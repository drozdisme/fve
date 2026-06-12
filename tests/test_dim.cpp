#include "../model/dim/dim.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

namespace {
IMat mul(const IMat& a, const IMat& b) {
    int m = a.size(), k = b.size(), n = b[0].size();
    IMat c(m, std::vector<long long>(n, 0));
    for (int i = 0; i < m; i++)
        for (int t = 0; t < k; t++)
            for (int j = 0; j < n; j++) c[i][j] += a[i][t] * b[t][j];
    return c;
}
bool diag_ok(const IMat& d) {
    int m = d.size(), n = d[0].size();
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            if (i != j && d[i][j] != 0) return false;
    return true;
}
}

void test_dim() {
    cur = "dim";
    IMat A = {{2, 4, 4}, {-6, 6, 12}, {10, 4, 16}};
    Smith s = smith_normal_form(A);
    IMat uav = mul(mul(s.u, A), s.v);
    CHECK(diag_ok(uav));
    CHECK(uav[0][0] == s.d[0][0] && uav[1][1] == s.d[1][1]);
    CHECK(s.rank == 3);
    CHECK(s.d[0][0] > 0 && s.d[1][1] % s.d[0][0] == 0 && s.d[2][2] % s.d[1][1] == 0);

    IMat B = {{2, 0}, {0, 0}};
    Smith s2 = smith_normal_form(B);
    CHECK(s2.rank == 1);

    DimGraph g;
    int F = g.sym("F"), M = g.sym("m"), A2 = g.sym("a"), L = g.sym("L"), T = g.sym("t");
    g.set_known(M, Dim::base(0));
    g.set_known(L, Dim::base(1));
    g.set_known(T, Dim::base(2));
    g.add_combo(A2, {{L, 1}, {T, -2}});
    g.add_combo(F, {{M, 1}, {A2, 1}});
    DimResult r = infer_dims(g);
    CHECK(r.consistent);
    CHECK(r.dims["a"].e[1] == 1 && r.dims["a"].e[2] == -2);
    CHECK(r.dims["F"].e[0] == 1 && r.dims["F"].e[1] == 1 && r.dims["F"].e[2] == -2);

    DimGraph g2;
    int X = g2.sym("x"), Y = g2.sym("y"), Z = g2.sym("z");
    g2.set_known(Y, Dim::base(1));
    g2.set_known(Z, Dim::base(2));
    g2.add_eq(X, Y);
    g2.add_eq(X, Z);
    DimResult r2 = infer_dims(g2);
    CHECK(!r2.consistent);

    DimGraph g3;
    int P = g3.sym("p"), Q = g3.sym("q");
    g3.set_known(P, Dim::base(1));
    g3.add_eq(P, Q);
    DimResult r3 = infer_dims(g3);
    CHECK(r3.consistent);
    CHECK(r3.dims["q"].e[1] == 1);
}
