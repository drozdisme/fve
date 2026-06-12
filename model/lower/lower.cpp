#include "lower.hpp"
#include "../../extractor/formula/formula.hpp"
#include <cmath>
#include <set>

namespace fve {

namespace {

struct Lifter {
    Workbook& wb;
    Oracle& orc;
    std::map<CellId, Nodep> memo;
    std::set<CellId> visiting;
    std::map<std::string, double>& base;
    int holes = 0;

    Lifter(Workbook& w, Oracle& o, std::map<std::string, double>& b)
        : wb(w), orc(o), base(b) {}

    Nodep mk_hole(double v) {
        (void)v;
        return hole("h" + std::to_string(holes++), 0, Ival::entire(), 'B', {});
    }

    Nodep lower_cell(const CellId& id) {
        auto m = memo.find(id);
        if (m != memo.end()) return m->second;
        if (visiting.count(id)) return mk_hole(0);
        visiting.insert(id);
        Nodep out;
        const Sheet& sh = wb.sheets[id.sheet];
        const Cell* c = sh.at(id.row, id.col);
        if (!c) {
            out = konst(0.0);
        } else if (!c->formula.empty()) {
            XParse p = parse_formula(c->formula);
            out = p.ok ? lower_x(p.ast, id.sheet) : mk_hole(c->value);
        } else if (c->has_value && c->text.empty()) {
            std::string nm = wb.key(id.sheet, id.row, id.col);
            base[nm] = c->value;
            out = var(nm, Dim::none(), Ival::entire());
        } else {
            out = konst(0.0);
        }
        visiting.erase(id);
        memo[id] = out;
        return out;
    }

    Nodep ref_node(const Ref& r, int cur) {
        Ref rr = r;
        if (rr.sheet < 0) rr.sheet = cur;
        return lower_cell({rr.sheet, rr.row, rr.col});
    }

    void expand(const XAstp& a, int cur, std::vector<Nodep>& out) {
        if (a->kind == XKind::Range) {
            int sh = a->sheet.empty() ? cur : wb.sheet_index(a->sheet);
            if (sh < 0) sh = cur;
            int r0 = std::min(a->ref.row, a->ref_end.row), r1 = std::max(a->ref.row, a->ref_end.row);
            int c0 = std::min(a->ref.col, a->ref_end.col), c1 = std::max(a->ref.col, a->ref_end.col);
            for (int r = r0; r <= r1; r++)
                for (int c = c0; c <= c1; c++) out.push_back(lower_cell({sh, r, c}));
        } else {
            out.push_back(lower_x(a, cur));
        }
    }

    Nodep fold(Op op, std::vector<Nodep> xs) {
        if (xs.empty()) return konst(0.0);
        Nodep acc = xs[0];
        for (size_t i = 1; i < xs.size(); i++) acc = oper(op, {acc, xs[i]});
        return acc;
    }

    Nodep fold_call(const std::string& fn, std::vector<Nodep> xs) {
        if (xs.empty()) return konst(0.0);
        Nodep acc = xs[0];
        for (size_t i = 1; i < xs.size(); i++) acc = call(fn, {acc, xs[i]});
        return acc;
    }

    Nodep lower_x(const XAstp& a, int cur) {
        switch (a->kind) {
            case XKind::Num: return konst(a->num);
            case XKind::Str: return mk_hole(0);
            case XKind::Cell: {
                Ref r = a->ref;
                r.sheet = a->sheet.empty() ? -1 : wb.sheet_index(a->sheet);
                return ref_node(r, cur);
            }
            case XKind::Range: return mk_hole(0);
            case XKind::NameRef: {
                auto it = wb.names.find(a->name);
                if (it != wb.names.end()) return ref_node(it->second.target, cur);
                return mk_hole(0);
            }
            case XKind::Un: {
                Nodep x = lower_x(a->args[0], cur);
                if (a->op == "-") return oper(Op::Neg, {x});
                if (a->op == "%") return oper(Op::Mul, {x, konst(0.01)});
                return x;
            }
            case XKind::Bin: {
                Nodep x = lower_x(a->args[0], cur), y = lower_x(a->args[1], cur);
                const std::string& o = a->op;
                if (o == "+") return oper(Op::Add, {x, y});
                if (o == "-") return oper(Op::Sub, {x, y});
                if (o == "*") return oper(Op::Mul, {x, y});
                if (o == "/") return oper(Op::Div, {x, y});
                if (o == "^") return oper(Op::Pow, {x, y});
                return mk_hole(0);
            }
            case XKind::Call: {
                const std::string& f = a->fn;
                if (f == "SUM") {
                    std::vector<Nodep> xs;
                    for (auto& arg : a->args) expand(arg, cur, xs);
                    return fold(Op::Add, xs);
                }
                if (f == "AVERAGE") {
                    std::vector<Nodep> xs;
                    for (auto& arg : a->args) expand(arg, cur, xs);
                    Nodep s = fold(Op::Add, xs);
                    return oper(Op::Div, {s, konst((double)xs.size())});
                }
                if (f == "MIN" || f == "MAX") {
                    std::vector<Nodep> xs;
                    for (auto& arg : a->args) expand(arg, cur, xs);
                    return fold_call(f == "MIN" ? "min" : "max", xs);
                }
                if (f == "PRODUCT") {
                    std::vector<Nodep> xs;
                    for (auto& arg : a->args) expand(arg, cur, xs);
                    return fold(Op::Mul, xs);
                }
                if (f == "SUMSQ") {
                    std::vector<Nodep> xs;
                    for (auto& arg : a->args) expand(arg, cur, xs);
                    std::vector<Nodep> sq;
                    for (auto& x : xs) sq.push_back(oper(Op::Mul, {x, x}));
                    return fold(Op::Add, sq);
                }
                if (f == "ABS") return call("abs", {lower_x(a->args[0], cur)});
                if (f == "SQRT") return oper(Op::Sqrt, {lower_x(a->args[0], cur)});
                if (f == "EXP") return oper(Op::Exp, {lower_x(a->args[0], cur)});
                if (f == "LN") return oper(Op::Log, {lower_x(a->args[0], cur)});
                if (f == "LOG" || f == "LOG10") {
                    Nodep x = oper(Op::Log, {lower_x(a->args[0], cur)});
                    if (f == "LOG" && a->args.size() > 1)
                        return oper(Op::Div, {x, oper(Op::Log, {lower_x(a->args[1], cur)})});
                    return oper(Op::Div, {x, oper(Op::Log, {konst(10.0)})});
                }
                if (f == "SIN") return oper(Op::Sin, {lower_x(a->args[0], cur)});
                if (f == "COS") return oper(Op::Cos, {lower_x(a->args[0], cur)});
                if (f == "TAN") return oper(Op::Tan, {lower_x(a->args[0], cur)});
                if (f == "DEGREES") return oper(Op::Mul, {lower_x(a->args[0], cur), konst(180.0 / 3.14159265358979311599796346854)});
                if (f == "RADIANS") return oper(Op::Mul, {lower_x(a->args[0], cur), konst(3.14159265358979311599796346854 / 180.0)});
                if (f == "POWER") return oper(Op::Pow, {lower_x(a->args[0], cur), lower_x(a->args[1], cur)});
                if (f == "PI") return konst(3.14159265358979311599796346854);
                if (f == "IF") {
                    Nodep t = lower_x(a->args[1], cur);
                    Nodep e = a->args.size() > 2 ? lower_x(a->args[2], cur) : konst(0.0);
                    return disj({t, e});
                }
                if (f == "IFERROR") {
                    Nodep t = lower_x(a->args[0], cur);
                    Nodep e = a->args.size() > 1 ? lower_x(a->args[1], cur) : konst(0.0);
                    return disj({t, e});
                }
                return mk_hole(0);
            }
        }
        return mk_hole(0);
    }
};

}

Lowered lower_target(Workbook& wb, Oracle& orc, const CellId& target) {
    Lowered out;
    Lifter lf(wb, orc, out.input_base);
    out.root = lf.lower_cell(target);
    out.holes = lf.holes;
    out.ok = out.root != nullptr;
    return out;
}

}
