#include "types.hpp"
#include <sstream>

namespace fve {

namespace {
const char* SYM[7] = {"kg", "m", "s", "A", "K", "mol", "cd"};
}

bool Dim::dimensionless() const {
    for (int i = 0; i < 7; i++)
        if (e[i] != 0) return false;
    return true;
}

std::string Dim::str() const {
    if (dimensionless()) return "1";
    std::ostringstream o;
    bool first = true;
    for (int i = 0; i < 7; i++) {
        if (e[i] == 0) continue;
        if (!first) o << "*";
        o << SYM[i];
        if (e[i] != 1) o << "^" << e[i];
        first = false;
    }
    return o.str();
}

bool operator==(const Dim& a, const Dim& b) { return a.e == b.e; }
bool operator!=(const Dim& a, const Dim& b) { return !(a == b); }

Dim mul(const Dim& a, const Dim& b) {
    Dim d;
    for (int i = 0; i < 7; i++) d.e[i] = a.e[i] + b.e[i];
    return d;
}

Dim div(const Dim& a, const Dim& b) {
    Dim d;
    for (int i = 0; i < 7; i++) d.e[i] = a.e[i] - b.e[i];
    return d;
}

Dim powd(const Dim& a, int n) {
    Dim d;
    for (int i = 0; i < 7; i++) d.e[i] = a.e[i] * n;
    return d;
}

bool compatible(const Dim& a, const Dim& b) { return a == b; }

}
