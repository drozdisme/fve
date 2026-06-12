#include "ocr.hpp"
#include "../formula/formula.hpp"
#include <algorithm>

namespace fve {

std::vector<Image> extract_images(const Zip& z) {
    std::vector<Image> out;
    for (const auto& name : z.list())
        if (name.find("xl/media/") == 0 || name.find("/media/") != std::string::npos) {
            const auto* b = z.bytes(name);
            if (b) out.push_back({name, *b});
        }
    return out;
}

namespace {

Nodep low(const XAstp& a, bool& ok) {
    if (!a) { ok = false; return konst(0.0); }
    switch (a->kind) {
        case XKind::Num: return konst(a->num);
        case XKind::Str: ok = false; return konst(0.0);
        case XKind::Cell: return var(a1(a->ref.row, a->ref.col), Dim::none(), Ival::entire());
        case XKind::NameRef: return var(a->name, Dim::none(), Ival::entire());
        case XKind::Range: ok = false; return konst(0.0);
        case XKind::Un: {
            Nodep x = low(a->args[0], ok);
            if (a->op == "-") return oper(Op::Neg, {x});
            if (a->op == "%") return oper(Op::Mul, {x, konst(0.01)});
            return x;
        }
        case XKind::Bin: {
            Nodep x = low(a->args[0], ok), y = low(a->args[1], ok);
            const std::string& o = a->op;
            if (o == "+") return oper(Op::Add, {x, y});
            if (o == "-") return oper(Op::Sub, {x, y});
            if (o == "*") return oper(Op::Mul, {x, y});
            if (o == "/") return oper(Op::Div, {x, y});
            if (o == "^") return oper(Op::Pow, {x, y});
            ok = false;
            return konst(0.0);
        }
        case XKind::Call: {
            const std::string& f = a->fn;
            if (f == "SQRT") return oper(Op::Sqrt, {low(a->args[0], ok)});
            if (f == "EXP") return oper(Op::Exp, {low(a->args[0], ok)});
            if (f == "LN") return oper(Op::Log, {low(a->args[0], ok)});
            if (f == "SIN") return oper(Op::Sin, {low(a->args[0], ok)});
            if (f == "COS") return oper(Op::Cos, {low(a->args[0], ok)});
            if (f == "TAN") return oper(Op::Tan, {low(a->args[0], ok)});
            if (f == "ABS") return call("abs", {low(a->args[0], ok)});
            if (f == "POWER") return oper(Op::Pow, {low(a->args[0], ok), low(a->args[1], ok)});
            ok = false;
            return konst(0.0);
        }
    }
    ok = false;
    return konst(0.0);
}

}

Nodep lower_symbolic(const std::string& formula, bool& ok) {
    XParse p = parse_formula(formula);
    if (!p.ok) { ok = false; return konst(0.0); }
    ok = true;
    Nodep n = low(p.ast, ok);
    return n;
}

Bundle bundle_from_lattice(const Lattice& lat, int max_cands, double floor) {
    struct Cand {
        std::string s;
        double conf;
    };
    std::vector<Cand> cur{{"", 1.0}};
    bool saw_unknown = false;
    for (const auto& slot : lat.slots) {
        std::vector<Cand> next;
        for (const auto& c : cur) {
            for (const auto& alt : slot) {
                if (alt.tok == "?") { saw_unknown = true; continue; }
                next.push_back({c.s + alt.tok, c.conf * alt.conf});
            }
        }
        std::sort(next.begin(), next.end(), [](const Cand& a, const Cand& b) { return a.conf > b.conf; });
        if ((int)next.size() > max_cands) next.resize(max_cands);
        cur = next;
    }

    Bundle b;
    for (const auto& c : cur) {
        if (c.conf < floor) continue;
        bool ok = false;
        Nodep n = lower_symbolic(c.s, ok);
        if (ok) b.add(n, c.conf, "ocr:" + c.s);
    }
    if (saw_unknown || b.size() == 0) b.mark_unknown(Ival::entire());
    b.normalize();
    b.prune(floor);
    return b;
}

}
