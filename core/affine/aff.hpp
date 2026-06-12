#pragma once
#include "../interval/ival.hpp"
#include <map>

namespace fve {

class Aff {
public:
    Aff() : c_(0.0), err_(0.0) {}
    explicit Aff(double v) : c_(v), err_(0.0) {}

    static Aff sym(double center, double radius);
    static Aff from_ival(const Ival& x);

    double center() const { return c_; }
    double err() const { return err_; }
    double rad() const;
    Ival to_ival() const;

    const std::map<int, double>& terms() const { return t_; }

    friend Aff operator+(const Aff& a, const Aff& b);
    friend Aff operator-(const Aff& a, const Aff& b);
    friend Aff operator*(const Aff& a, const Aff& b);
    friend Aff operator*(double s, const Aff& a);
    friend Aff neg(const Aff& a);
    friend Aff add_const(const Aff& a, double s);

private:
    double c_;
    std::map<int, double> t_;
    double err_;
    static int next_;
};

Aff aff_inv(const Aff& a);
Aff aff_div(const Aff& a, const Aff& b);

}
