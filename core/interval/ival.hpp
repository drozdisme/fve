#pragma once
#include <string>

namespace fve {

struct Ival {
    double lo, hi;

    static Ival point(double x) { return {x, x}; }
    static Ival of(double a, double b) { return a <= b ? Ival{a, b} : Ival{b, a}; }
    static Ival empty();
    static Ival entire();
    static Ival nonneg();

    bool is_empty() const;
    bool is_entire() const;
    bool contains(double x) const;
    bool subset(const Ival& o) const;
    double width() const;
    double mid() const;
    double mag() const;
};

bool operator==(const Ival& a, const Ival& b);

Ival neg(const Ival& a);
Ival add(const Ival& a, const Ival& b);
Ival sub(const Ival& a, const Ival& b);
Ival mul(const Ival& a, const Ival& b);
Ival divi(const Ival& a, const Ival& b);
Ival sqr(const Ival& a);
Ival ipow(const Ival& a, int n);
Ival rpow(const Ival& a, double p);
Ival isqrt(const Ival& a);
Ival ilog(const Ival& a);
Ival iexp(const Ival& a);
Ival isin(const Ival& a);
Ival icos(const Ival& a);
Ival itan(const Ival& a);

Ival hull(const Ival& a, const Ival& b);
Ival meet(const Ival& a, const Ival& b);

std::string to_str(const Ival& a);

}
