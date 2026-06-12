#include "enclosure.hpp"
#include <algorithm>
#include <cmath>

namespace fve {

namespace {

Ival apply_op(Op o, const std::vector<Ival>& a) {
    switch (o) {
        case Op::Add: return add(a[0], a[1]);
        case Op::Sub: return sub(a[0], a[1]);
        case Op::Mul: return mul(a[0], a[1]);
        case Op::Div: return divi(a[0], a[1]);
        case Op::Neg: return neg(a[0]);
        case Op::Pow: return rpow(a[0], a[1].lo);
        case Op::Sqrt: return isqrt(a[0]);
        case Op::Log: return ilog(a[0]);
        case Op::Exp: return iexp(a[0]);
        case Op::Sin: return isin(a[0]);
        case Op::Cos: return icos(a[0]);
        case Op::Tan: return itan(a[0]);
    }
    return Ival::entire();
}

Ival call_fn(const std::string& fn, const std::vector<Ival>& a) {
    if (fn == "max" && a.size() == 2) return Ival::of(std::max(a[0].lo, a[1].lo), std::max(a[0].hi, a[1].hi));
    if (fn == "min" && a.size() == 2) return Ival::of(std::min(a[0].lo, a[1].lo), std::min(a[0].hi, a[1].hi));
    if (fn == "abs" && a.size() == 1) {
        double m = a[0].mag();
        double lo = (a[0].lo <= 0 && 0 <= a[0].hi) ? 0.0 : std::min(std::fabs(a[0].lo), std::fabs(a[0].hi));
        return Ival::of(lo, m);
    }
    Ival r = Ival::empty();
    for (const auto& x : a) r = hull(r, x);
    return r;
}

Ival rec(const Nodep& n, const Box& box, const HoleTable& holes,
         double& residual, std::vector<Trace>* tr) {
    Ival out;
    bool holed = false;
    char regime = 'A';
    switch (n->kind) {
        case Kind::Const: out = Ival::point(n->val); break;
        case Kind::Var: {
            auto it = box.find(n->sym);
            out = (it != box.end()) ? it->second : n->phys;
            break;
        }
        case Kind::Oper: {
            std::vector<Ival> a;
            for (const auto& c : n->args) a.push_back(rec(c, box, holes, residual, tr));
            out = apply_op(n->op, a);
            break;
        }
        case Kind::Call: {
            std::vector<Ival> a;
            for (const auto& c : n->args) a.push_back(rec(c, box, holes, residual, tr));
            out = call_fn(n->fn, a);
            break;
        }
        case Kind::Hole: {
            holed = true;
            regime = n->regime;
            std::vector<Ival> a;
            for (const auto& c : n->args) a.push_back(rec(c, box, holes, residual, tr));
            WrapResult w = holes.eval(n->hole, n->phys, a);
            out = w.encl;
            residual += w.residual;
            break;
        }
        case Kind::Disj: {
            std::vector<Ival> cs;
            for (const auto& c : n->cands) cs.push_back(rec(c, box, holes, residual, tr));
            out = bundle_hull(cs);
            break;
        }
    }
    if (tr) {
        std::string label;
        switch (n->kind) {
            case Kind::Const: label = "const"; break;
            case Kind::Var: label = "var " + n->sym; break;
            case Kind::Oper: label = op_name(n->op); break;
            case Kind::Call: label = "call " + n->fn; break;
            case Kind::Hole: label = "hole " + n->hole; break;
            case Kind::Disj: label = "disj"; break;
        }
        tr->push_back({n->hash, label, out, out.width(), holed, regime});
    }
    return out;
}

}

Ival eval_ival(const Nodep& n, const Box& box, const HoleTable& holes,
               double& residual) {
    return rec(n, box, holes, residual, nullptr);
}

EvalResult eval(const Program& p) {
    EvalResult r;
    r.residual = 0.0;
    r.ms = rec(p.root, p.box, p.holes, r.residual, &r.trace);
    r.residual += p.ks;
    return r;
}

namespace {
Aff arec(const Nodep& n, const std::map<std::string, Aff>& env,
         const HoleTable& holes, double& residual) {
    switch (n->kind) {
        case Kind::Const: return Aff(n->val);
        case Kind::Var: {
            auto it = env.find(n->sym);
            return it != env.end() ? it->second : Aff::from_ival(n->phys);
        }
        case Kind::Oper: {
            std::vector<Aff> a;
            for (const auto& c : n->args) a.push_back(arec(c, env, holes, residual));
            switch (n->op) {
                case Op::Add: return a[0] + a[1];
                case Op::Sub: return a[0] - a[1];
                case Op::Mul: return a[0] * a[1];
                case Op::Div: return aff_div(a[0], a[1]);
                case Op::Neg: return neg(a[0]);
                default: {
                    std::vector<Ival> iv;
                    for (auto& x : a) iv.push_back(x.to_ival());
                    return Aff::from_ival(apply_op(n->op, iv));
                }
            }
        }
        case Kind::Hole: {
            std::vector<Ival> a;
            for (const auto& c : n->args) {
                double rr = 0.0;
                a.push_back(arec(c, env, holes, rr).to_ival());
                residual += rr;
            }
            WrapResult w = holes.eval(n->hole, n->phys, a);
            residual += w.residual;
            return Aff::from_ival(w.encl);
        }
        case Kind::Disj: {
            std::vector<Ival> cs;
            for (const auto& c : n->cands) {
                double rr = 0.0;
                cs.push_back(arec(c, env, holes, rr).to_ival());
            }
            return Aff::from_ival(bundle_hull(cs));
        }
        default: {
            std::vector<Ival> a;
            for (const auto& c : n->args) a.push_back(arec(c, env, holes, residual).to_ival());
            return Aff::from_ival(call_fn(n->fn, a));
        }
    }
}
}

Ival eval_aff(const Nodep& n, const std::map<std::string, Aff>& env,
              const HoleTable& holes, double& residual) {
    return arec(n, env, holes, residual).to_ival();
}

double total_width(const std::vector<Trace>& t) {
    double w = 0.0;
    for (const auto& e : t) w = std::max(w, e.width);
    return w;
}

}
