#include "ival.hpp"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <limits>
#include <sstream>

#pragma STDC FENV_ACCESS ON

namespace fve {

namespace {

const double INF = std::numeric_limits<double>::infinity();
const double PI = 3.14159265358979311599796346854;
const int SLACK = 2;

inline double down(double x) {
    if (std::isinf(x) || std::isnan(x)) return x;
    for (int i = 0; i < SLACK; i++) x = std::nextafter(x, -INF);
    return x;
}
inline double up(double x) {
    if (std::isinf(x) || std::isnan(x)) return x;
    for (int i = 0; i < SLACK; i++) x = std::nextafter(x, INF);
    return x;
}

struct Mode {
    int prev;
    explicit Mode(int m) : prev(std::fegetround()) { std::fesetround(m); }
    ~Mode() { std::fesetround(prev); }
};

inline double dmin4(double a, double b, double c, double d) {
    return std::min(std::min(a, b), std::min(c, d));
}
inline double dmax4(double a, double b, double c, double d) {
    return std::max(std::max(a, b), std::max(c, d));
}

bool crit_in(double base, double period, double lo, double hi) {
    double k0 = std::ceil((lo - base) / period);
    double v = base + k0 * period;
    return v >= lo && v <= hi;
}

}

Ival Ival::empty() { return {INF, -INF}; }
Ival Ival::entire() { return {-INF, INF}; }
Ival Ival::nonneg() { return {0.0, INF}; }

bool Ival::is_empty() const { return lo > hi; }
bool Ival::is_entire() const { return lo == -INF && hi == INF; }
bool Ival::contains(double x) const { return !is_empty() && lo <= x && x <= hi; }
bool Ival::subset(const Ival& o) const {
    if (is_empty()) return true;
    if (o.is_empty()) return false;
    return o.lo <= lo && hi <= o.hi;
}
double Ival::width() const { return is_empty() ? 0.0 : up(hi - lo); }
double Ival::mid() const { return is_empty() ? std::nan("") : 0.5 * (lo + hi); }
double Ival::mag() const { return is_empty() ? 0.0 : std::max(std::fabs(lo), std::fabs(hi)); }

bool operator==(const Ival& a, const Ival& b) {
    return (a.is_empty() && b.is_empty()) || (a.lo == b.lo && a.hi == b.hi);
}

Ival neg(const Ival& a) { return a.is_empty() ? a : Ival{-a.hi, -a.lo}; }

Ival add(const Ival& a, const Ival& b) {
    if (a.is_empty() || b.is_empty()) return Ival::empty();
    Mode m(FE_DOWNWARD);
    volatile double lo = a.lo + b.lo;
    std::fesetround(FE_UPWARD);
    volatile double hi = a.hi + b.hi;
    return {lo, hi};
}

Ival sub(const Ival& a, const Ival& b) {
    if (a.is_empty() || b.is_empty()) return Ival::empty();
    Mode m(FE_DOWNWARD);
    volatile double lo = a.lo - b.hi;
    std::fesetround(FE_UPWARD);
    volatile double hi = a.hi - b.lo;
    return {lo, hi};
}

Ival mul(const Ival& a, const Ival& b) {
    if (a.is_empty() || b.is_empty()) return Ival::empty();
    double lo, hi;
    {
        Mode m(FE_DOWNWARD);
        lo = dmin4(a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi);
    }
    {
        Mode m(FE_UPWARD);
        hi = dmax4(a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi);
    }
    return {lo, hi};
}

Ival divi(const Ival& a, const Ival& b) {
    if (a.is_empty() || b.is_empty()) return Ival::empty();
    if (b.lo <= 0.0 && 0.0 <= b.hi) return Ival::entire();
    double lo, hi;
    {
        Mode m(FE_DOWNWARD);
        lo = dmin4(a.lo / b.lo, a.lo / b.hi, a.hi / b.lo, a.hi / b.hi);
    }
    {
        Mode m(FE_UPWARD);
        hi = dmax4(a.lo / b.lo, a.lo / b.hi, a.hi / b.lo, a.hi / b.hi);
    }
    return {lo, hi};
}

Ival sqr(const Ival& a) {
    if (a.is_empty()) return a;
    double l = std::fabs(a.lo), h = std::fabs(a.hi);
    double mx = std::max(l, h);
    double mn = (a.lo <= 0.0 && 0.0 <= a.hi) ? 0.0 : std::min(l, h);
    Mode m(FE_DOWNWARD);
    volatile double lo = mn * mn;
    std::fesetround(FE_UPWARD);
    volatile double hi = mx * mx;
    return {lo, hi};
}

Ival ipow(const Ival& a, int n) {
    if (a.is_empty()) return a;
    if (n == 0) return Ival::point(1.0);
    if (n < 0) return divi(Ival::point(1.0), ipow(a, -n));
    Ival r = Ival::point(1.0);
    Ival base = a;
    int e = n;
    while (e > 0) {
        if (e & 1) r = mul(r, base);
        e >>= 1;
        if (e) base = mul(base, base);
    }
    if (n % 2 == 0 && a.lo < 0.0 && a.hi > 0.0) r.lo = 0.0;
    return r;
}

Ival rpow(const Ival& a, double p) {
    if (a.is_empty()) return a;
    double r = std::floor(p);
    if (r == p && std::fabs(p) < 1e9) return ipow(a, (int)p);
    double lo = std::max(a.lo, 0.0);
    double hi = std::max(a.hi, 0.0);
    if (a.hi < 0.0) return Ival::empty();
    double fl = std::pow(lo, p), fh = std::pow(hi, p);
    if (p >= 0.0) return {down(std::min(fl, fh)), up(std::max(fl, fh))};
    return {down(std::min(fl, fh)), up(std::max(fl, fh))};
}

Ival isqrt(const Ival& a) {
    if (a.is_empty()) return a;
    if (a.hi < 0.0) return Ival::empty();
    double lo = std::max(a.lo, 0.0);
    double rl, rh;
    {
        Mode m(FE_DOWNWARD);
        rl = std::sqrt(lo);
    }
    {
        Mode m(FE_UPWARD);
        rh = std::sqrt(a.hi);
    }
    return {rl, rh};
}

Ival ilog(const Ival& a) {
    if (a.is_empty()) return a;
    if (a.hi <= 0.0) return Ival::empty();
    double lo = (a.lo <= 0.0) ? -INF : down(std::log(a.lo));
    double hi = up(std::log(a.hi));
    return {lo, hi};
}

Ival iexp(const Ival& a) {
    if (a.is_empty()) return a;
    return {down(std::exp(a.lo)), up(std::exp(a.hi))};
}

Ival isin(const Ival& a) {
    if (a.is_empty()) return a;
    if (a.width() >= 2.0 * PI) return {-1.0, 1.0};
    double sl = std::sin(a.lo), sh = std::sin(a.hi);
    double lo = std::min(sl, sh), hi = std::max(sl, sh);
    if (crit_in(PI / 2.0, 2.0 * PI, a.lo, a.hi)) hi = 1.0;
    if (crit_in(-PI / 2.0, 2.0 * PI, a.lo, a.hi)) lo = -1.0;
    return {std::max(-1.0, down(lo)), std::min(1.0, up(hi))};
}

Ival icos(const Ival& a) {
    if (a.is_empty()) return a;
    if (a.width() >= 2.0 * PI) return {-1.0, 1.0};
    double cl = std::cos(a.lo), ch = std::cos(a.hi);
    double lo = std::min(cl, ch), hi = std::max(cl, ch);
    if (crit_in(0.0, 2.0 * PI, a.lo, a.hi)) hi = 1.0;
    if (crit_in(PI, 2.0 * PI, a.lo, a.hi)) lo = -1.0;
    return {std::max(-1.0, down(lo)), std::min(1.0, up(hi))};
}

Ival itan(const Ival& a) {
    if (a.is_empty()) return a;
    if (a.width() >= PI) return Ival::entire();
    if (crit_in(PI / 2.0, PI, a.lo, a.hi)) return Ival::entire();
    return {down(std::tan(a.lo)), up(std::tan(a.hi))};
}

Ival hull(const Ival& a, const Ival& b) {
    if (a.is_empty()) return b;
    if (b.is_empty()) return a;
    return {std::min(a.lo, b.lo), std::max(a.hi, b.hi)};
}

Ival meet(const Ival& a, const Ival& b) {
    if (a.is_empty() || b.is_empty()) return Ival::empty();
    double lo = std::max(a.lo, b.lo), hi = std::min(a.hi, b.hi);
    return lo <= hi ? Ival{lo, hi} : Ival::empty();
}

std::string to_str(const Ival& a) {
    if (a.is_empty()) return "[empty]";
    std::ostringstream o;
    o.precision(15);
    o << "[" << a.lo << ", " << a.hi << "]";
    return o.str();
}

}
