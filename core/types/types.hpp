#pragma once
#include "../interval/ival.hpp"
#include <array>
#include <string>

namespace fve {

struct Dim {
    std::array<int, 7> e{};

    static Dim none() { return Dim{}; }
    static Dim base(int i) {
        Dim d;
        if (i >= 0 && i < 7) d.e[i] = 1;
        return d;
    }
    bool dimensionless() const;
    std::string str() const;
};

bool operator==(const Dim& a, const Dim& b);
bool operator!=(const Dim& a, const Dim& b);
Dim mul(const Dim& a, const Dim& b);
Dim div(const Dim& a, const Dim& b);
Dim powd(const Dim& a, int n);
bool compatible(const Dim& a, const Dim& b);

struct Unit {
    std::string name;
    Dim dim;
    double scale = 1.0;
    double offset = 0.0;
    double to_si(double v) const { return v * scale + offset; }
    double from_si(double v) const { return (v - offset) / scale; }
};

struct Phys {
    Dim dim;
    Ival range;
    static Phys any(const Dim& d) { return {d, Ival::entire()}; }
    static Phys pos(const Dim& d) { return {d, Ival{0.0, Ival::entire().hi}}; }
    static Phys of(const Dim& d, double lo, double hi) { return {d, Ival::of(lo, hi)}; }
};

struct Symbol {
    std::string id;
    Dim dim;
    Ival phys = Ival::entire();
    std::string unit;
};

}
