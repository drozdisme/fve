#include "aff.hpp"
#include <cmath>

namespace fve {

int Aff::next_ = 1;

Aff Aff::sym(double center, double radius) {
    Aff a;
    a.c_ = center;
    if (radius != 0.0) a.t_[next_++] = std::fabs(radius);
    return a;
}

Aff Aff::from_ival(const Ival& x) {
    if (x.is_empty()) return Aff(std::nan(""));
    return sym(x.mid(), 0.5 * x.width());
}

double Aff::rad() const {
    double r = err_;
    for (const auto& kv : t_) r += std::fabs(kv.second);
    return r;
}

Ival Aff::to_ival() const {
    double r = rad();
    return add(Ival::point(c_), Ival::of(-r, r));
}

Aff operator+(const Aff& a, const Aff& b) {
    Aff r;
    r.c_ = a.c_ + b.c_;
    r.t_ = a.t_;
    for (const auto& kv : b.t_) r.t_[kv.first] += kv.second;
    r.err_ = a.err_ + b.err_;
    return r;
}

Aff neg(const Aff& a) {
    Aff r;
    r.c_ = -a.c_;
    for (const auto& kv : a.t_) r.t_[kv.first] = -kv.second;
    r.err_ = a.err_;
    return r;
}

Aff operator-(const Aff& a, const Aff& b) { return a + neg(b); }

Aff operator*(double s, const Aff& a) {
    Aff r;
    r.c_ = s * a.c_;
    for (const auto& kv : a.t_) r.t_[kv.first] = s * kv.second;
    r.err_ = std::fabs(s) * a.err_;
    return r;
}

Aff add_const(const Aff& a, double s) {
    Aff r = a;
    r.c_ += s;
    return r;
}

Aff operator*(const Aff& a, const Aff& b) {
    Aff r;
    r.c_ = a.c_ * b.c_;
    for (const auto& kv : a.t_) r.t_[kv.first] += b.c_ * kv.second;
    for (const auto& kv : b.t_) r.t_[kv.first] += a.c_ * kv.second;
    double ra = 0.0, rb = 0.0;
    for (const auto& kv : a.t_) ra += std::fabs(kv.second);
    for (const auto& kv : b.t_) rb += std::fabs(kv.second);
    r.err_ = a.err_ * (std::fabs(b.c_) + rb) + b.err_ * (std::fabs(a.c_) + ra) +
             a.err_ * b.err_ + ra * rb;
    return r;
}

Aff aff_inv(const Aff& a) {
    Ival x = a.to_ival();
    Ival r = divi(Ival::point(1.0), x);
    return Aff::from_ival(r);
}

Aff aff_div(const Aff& a, const Aff& b) { return a * aff_inv(b); }

}
