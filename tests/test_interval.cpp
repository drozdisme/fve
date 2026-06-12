#include "../core/interval/ival.hpp"
#include "framework.hpp"
#include <cmath>
#include <functional>
#include <vector>

using namespace fve;
using namespace fvetest;

namespace {

bool encloses(const Ival& iv, double v) {
    return !iv.is_empty() && iv.lo <= v && v <= iv.hi;
}

void inc_unary(Rng& rng, double lo, double hi, std::function<Ival(Ival)> f,
               std::function<double(double)> g, int n) {
    Ival box = Ival::of(lo, hi);
    Ival r = f(box);
    if (r.is_empty() || r.is_entire()) return;
    for (int i = 0; i < n; i++) {
        double x = rng.uniform(lo, hi);
        double y = g(x);
        if (std::isnan(y) || std::isinf(y)) continue;
        CHECK(encloses(r, y));
    }
}

void inc_binary(Rng& rng, double lo, double hi, std::function<Ival(Ival, Ival)> f,
                std::function<double(double, double)> g, int n) {
    for (int t = 0; t < 30; t++) {
        double a0 = rng.uniform(lo, hi), a1 = rng.uniform(lo, hi);
        double b0 = rng.uniform(lo, hi), b1 = rng.uniform(lo, hi);
        Ival A = Ival::of(a0, a1), B = Ival::of(b0, b1);
        Ival r = f(A, B);
        if (r.is_empty() || r.is_entire()) continue;
        for (int i = 0; i < n; i++) {
            double x = rng.uniform(A.lo, A.hi), y = rng.uniform(B.lo, B.hi);
            double v = g(x, y);
            if (std::isnan(v) || std::isinf(v)) continue;
            CHECK(encloses(r, v));
        }
    }
}

}

void test_interval() {
    cur = "interval";
    CHECK(add(Ival{1, 2}, Ival{3, 4}) == (Ival{4, 6}));
    CHECK(sub(Ival{1, 2}, Ival{3, 4}) == (Ival{-3, -1}));
    CHECK(mul(Ival{-1, 2}, Ival{-3, 4}) == (Ival{-6, 8}));
    CHECK(divi(Ival{1, 2}, Ival{-1, 1}).is_entire());
    CHECK(Ival::empty().is_empty());
    CHECK(neg(Ival{1, 2}) == (Ival{-2, -1}));
    CHECK(ipow(Ival{-2, 3}, 2) == (Ival{0, 9}));
    CHECK(ipow(Ival{2, 3}, 0) == Ival::point(1.0));
    CHECK(sqr(Ival{-3, 2}) == (Ival{0, 9}));
    CHECK(isqrt(Ival{-1, -0.5}).is_empty());
    CHECK(hull(Ival{1, 2}, Ival{5, 6}) == (Ival{1, 6}));
    CHECK(meet(Ival{1, 4}, Ival{3, 6}) == (Ival{3, 4}));
    CHECK(meet(Ival{1, 2}, Ival{5, 6}).is_empty());

    CHECK(encloses(isqrt(Ival{2, 2}), std::sqrt(2.0)));
    CHECK(encloses(iexp(Ival{1, 1}), std::exp(1.0)));
    CHECK(encloses(ilog(Ival{2, 2}), std::log(2.0)));
    CHECK(encloses(divi(Ival::point(1), Ival::point(3)), 1.0 / 3.0));

    Rng rng(12345);
    inc_unary(rng, 0.1, 10.0, isqrt, [](double x) { return std::sqrt(x); }, 50);
    inc_unary(rng, -3.0, 3.0, iexp, [](double x) { return std::exp(x); }, 50);
    inc_unary(rng, 0.1, 50.0, ilog, [](double x) { return std::log(x); }, 50);
    inc_unary(rng, -7.0, 7.0, isin, [](double x) { return std::sin(x); }, 80);
    inc_unary(rng, -7.0, 7.0, icos, [](double x) { return std::cos(x); }, 80);
    inc_unary(rng, -1.0, 1.0, itan, [](double x) { return std::tan(x); }, 50);

    inc_binary(rng, -10.0, 10.0, add, [](double x, double y) { return x + y; }, 40);
    inc_binary(rng, -10.0, 10.0, sub, [](double x, double y) { return x - y; }, 40);
    inc_binary(rng, -10.0, 10.0, mul, [](double x, double y) { return x * y; }, 40);
    inc_binary(rng, 1.0, 10.0, divi, [](double x, double y) { return x / y; }, 40);

    for (int t = 0; t < 200; t++) {
        double a = rng.uniform(0.1, 10), b = rng.uniform(0.1, 10);
        Ival X = Ival::of(std::min(a, b), std::max(a, b));
        Ival Xp = Ival::of(X.lo - rng.uniform(0, 2), X.hi + rng.uniform(0, 2));
        CHECK(isqrt(X).subset(isqrt(Xp)));
        CHECK(iexp(X).subset(iexp(Xp)));
    }
}
