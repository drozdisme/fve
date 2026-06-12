#include "ast.hpp"
#include <cstring>
#include <map>

namespace fve {

namespace {

void put_str(std::vector<uint8_t>& v, const std::string& s) {
    uint32_t n = (uint32_t)s.size();
    for (int i = 0; i < 4; i++) v.push_back(uint8_t(n >> (i * 8)));
    v.insert(v.end(), s.begin(), s.end());
}

void put_d(std::vector<uint8_t>& v, double d) {
    uint64_t b;
    std::memcpy(&b, &d, 8);
    for (int i = 0; i < 8; i++) v.push_back(uint8_t(b >> (i * 8)));
}

Digest compute_hash(const Node& n) {
    std::vector<uint8_t> v;
    v.push_back((uint8_t)n.kind);
    v.push_back((uint8_t)n.op);
    put_d(v, n.val);
    put_str(v, n.sym);
    put_str(v, n.fn);
    put_str(v, n.hole);
    v.push_back((uint8_t)n.regime);
    for (int e : n.dim.e)
        for (int i = 0; i < 4; i++) v.push_back(uint8_t(e >> (i * 8)));
    put_d(v, n.phys.lo);
    put_d(v, n.phys.hi);
    for (const auto& a : n.args) v.insert(v.end(), a->hash.begin(), a->hash.end());
    for (const auto& c : n.cands) v.insert(v.end(), c->hash.begin(), c->hash.end());
    return sha256(v);
}

Nodep finish(Node n) {
    n.hash = compute_hash(n);
    return std::make_shared<const Node>(std::move(n));
}

}

Nodep konst(double v, Dim dim) {
    Node n;
    n.kind = Kind::Const;
    n.val = v;
    n.dim = dim;
    n.phys = Ival::point(v);
    return finish(std::move(n));
}

Nodep var(const std::string& s, Dim dim, Ival phys) {
    Node n;
    n.kind = Kind::Var;
    n.sym = s;
    n.dim = dim;
    n.phys = phys;
    return finish(std::move(n));
}

Nodep oper(Op o, std::vector<Nodep> args) {
    Node n;
    n.kind = Kind::Oper;
    n.op = o;
    n.args = std::move(args);
    return finish(std::move(n));
}

Nodep call(const std::string& fn, std::vector<Nodep> args) {
    Node n;
    n.kind = Kind::Call;
    n.fn = fn;
    n.args = std::move(args);
    return finish(std::move(n));
}

Nodep hole(const std::string& id, int arity, Ival phys, char regime,
           std::vector<Nodep> args) {
    Node n;
    n.kind = Kind::Hole;
    n.hole = id;
    n.arity = arity;
    n.phys = phys;
    n.regime = regime;
    n.args = std::move(args);
    return finish(std::move(n));
}

Nodep disj(std::vector<Nodep> cands) {
    Node n;
    n.kind = Kind::Disj;
    n.cands = std::move(cands);
    return finish(std::move(n));
}

namespace {
struct OpName {
    Op o;
    const char* s;
};
const OpName OPS[] = {
    {Op::Add, "add"}, {Op::Sub, "sub"}, {Op::Mul, "mul"}, {Op::Div, "div"},
    {Op::Neg, "neg"}, {Op::Pow, "pow"}, {Op::Sqrt, "sqrt"}, {Op::Log, "log"},
    {Op::Exp, "exp"}, {Op::Sin, "sin"}, {Op::Cos, "cos"}, {Op::Tan, "tan"}};
}

std::string op_name(Op o) {
    for (const auto& e : OPS)
        if (e.o == o) return e.s;
    return "add";
}

bool op_from_name(const std::string& s, Op& out) {
    for (const auto& e : OPS)
        if (s == e.s) { out = e.o; return true; }
    return false;
}

int op_arity(Op o) {
    switch (o) {
        case Op::Neg:
        case Op::Sqrt:
        case Op::Log:
        case Op::Exp:
        case Op::Sin:
        case Op::Cos:
        case Op::Tan: return 1;
        default: return 2;
    }
}

JsonP to_json(const Nodep& n) {
    auto j = Json::mkobj();
    switch (n->kind) {
        case Kind::Const:
            j->obj["k"] = Json::mkstr("const");
            j->obj["v"] = Json::mknum(n->val);
            break;
        case Kind::Var:
            j->obj["k"] = Json::mkstr("var");
            j->obj["sym"] = Json::mkstr(n->sym);
            if (!n->phys.is_entire()) {
                auto p = Json::mkarr();
                p->arr.push_back(Json::mknum(n->phys.lo));
                p->arr.push_back(Json::mknum(n->phys.hi));
                j->obj["phys"] = p;
            }
            break;
        case Kind::Oper: {
            j->obj["k"] = Json::mkstr("op");
            j->obj["op"] = Json::mkstr(op_name(n->op));
            auto a = Json::mkarr();
            for (const auto& c : n->args) a->arr.push_back(to_json(c));
            j->obj["args"] = a;
            break;
        }
        case Kind::Call: {
            j->obj["k"] = Json::mkstr("call");
            j->obj["fn"] = Json::mkstr(n->fn);
            auto a = Json::mkarr();
            for (const auto& c : n->args) a->arr.push_back(to_json(c));
            j->obj["args"] = a;
            break;
        }
        case Kind::Hole: {
            j->obj["k"] = Json::mkstr("hole");
            j->obj["id"] = Json::mkstr(n->hole);
            j->obj["arity"] = Json::mknum(n->arity);
            j->obj["regime"] = Json::mkstr(std::string(1, n->regime));
            auto p = Json::mkarr();
            p->arr.push_back(Json::mknum(n->phys.lo));
            p->arr.push_back(Json::mknum(n->phys.hi));
            j->obj["phys"] = p;
            auto a = Json::mkarr();
            for (const auto& c : n->args) a->arr.push_back(to_json(c));
            j->obj["args"] = a;
            break;
        }
        case Kind::Disj: {
            j->obj["k"] = Json::mkstr("disj");
            auto a = Json::mkarr();
            for (const auto& c : n->cands) a->arr.push_back(to_json(c));
            j->obj["cands"] = a;
            break;
        }
    }
    return j;
}

Nodep from_json(const JsonP& j) {
    std::string k = j->str("k");
    if (k == "const") return konst(j->n("v"));
    if (k == "var") {
        Ival p = Ival::entire();
        if (j->has("phys")) {
            auto a = j->at("phys");
            p = Ival::of(a->arr[0]->num, a->arr[1]->num);
        }
        return var(j->str("sym"), Dim::none(), p);
    }
    if (k == "op") {
        Op o;
        op_from_name(j->str("op"), o);
        std::vector<Nodep> args;
        for (const auto& c : j->at("args")->arr) args.push_back(from_json(c));
        return oper(o, args);
    }
    if (k == "call") {
        std::vector<Nodep> args;
        for (const auto& c : j->at("args")->arr) args.push_back(from_json(c));
        return call(j->str("fn"), args);
    }
    if (k == "hole") {
        auto a = j->at("phys");
        Ival p = a->arr.size() >= 2 ? Ival::of(a->arr[0]->num, a->arr[1]->num)
                                    : Ival::entire();
        std::string rg = j->str("regime", "A");
        std::vector<Nodep> args;
        for (const auto& c : j->at("args")->arr) args.push_back(from_json(c));
        return hole(j->str("id"), (int)j->n("arity"), p, rg.empty() ? 'A' : rg[0],
                    args);
    }
    if (k == "disj") {
        std::vector<Nodep> cands;
        for (const auto& c : j->at("cands")->arr) cands.push_back(from_json(c));
        return disj(cands);
    }
    return konst(0.0);
}

std::string serialize(const Nodep& n) { return dump(to_json(n)); }
Nodep deserialize(const std::string& s) { return from_json(parse(s)); }

namespace {
int emit_slot(const Nodep& n, std::map<std::string, int>& seen,
              std::vector<Slot>& out) {
    std::string h = hex(n->hash);
    auto it = seen.find(h);
    if (it != seen.end()) return it->second;
    Slot s;
    s.kind = n->kind;
    s.op = n->op;
    s.val = n->val;
    s.sym = n->sym;
    s.fn = n->fn;
    s.hole = n->hole;
    s.hash = n->hash;
    for (const auto& a : n->args) s.args.push_back(emit_slot(a, seen, out));
    for (const auto& c : n->cands) s.cands.push_back(emit_slot(c, seen, out));
    int idx = (int)out.size();
    out.push_back(s);
    seen[h] = idx;
    return idx;
}
}

std::vector<Slot> linearize(const Nodep& n) {
    std::vector<Slot> out;
    std::map<std::string, int> seen;
    emit_slot(n, seen, out);
    return out;
}

}
