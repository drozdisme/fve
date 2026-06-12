#include "../core/affine/aff.hpp"
#include "framework.hpp"
#include <cmath>

using namespace fve;
using namespace fvetest;

void test_affine() {
    cur = "affine";
    Aff x = Aff::sym(5.0, 2.0);
    Ival xi = x.to_ival();
    CHECK(xi.lo <= 3.0 && xi.hi >= 7.0);

    Aff d = x - x;
    CHECK(d.to_ival().contains(0.0));
    CHECK(d.to_ival().width() < 1e-12);

    Aff y = Aff::sym(10.0, 1.0);
    Aff s = x + y;
    CHECK(s.to_ival().contains(15.0));
    NEAR(s.center(), 15.0, 1e-12);

    Aff p = 3.0 * x;
    CHECK(p.to_ival().contains(15.0));
    NEAR(p.center(), 15.0, 1e-12);

    Aff prod = x * y;
    CHECK(prod.to_ival().contains(50.0));

    Aff c = add_const(x, 2.0);
    NEAR(c.center(), 7.0, 1e-12);

    Aff zero_w = Aff(4.0);
    CHECK(zero_w.to_ival().width() < 1e-15);

    Rng rng(999);
    for (int t = 0; t < 300; t++) {
        double cx = rng.uniform(-5, 5), rx = rng.uniform(0, 3);
        Aff a = Aff::sym(cx, rx);
        Aff expr = a * a - a;
        Ival r = expr.to_ival();
        for (int i = 0; i < 20; i++) {
            double e = rng.uniform(-1, 1);
            double v = cx + rx * e;
            double truth = v * v - v;
            CHECK(r.lo - 1e-9 <= truth && truth <= r.hi + 1e-9);
        }
    }

    Aff a = Aff::sym(0.0, 1.0);
    Aff sub_self = a - a;
    Ival self_iv = sub_self.to_ival();
    Ival naive = sub(a.to_ival(), a.to_ival());
    CHECK(self_iv.width() < naive.width());
}
