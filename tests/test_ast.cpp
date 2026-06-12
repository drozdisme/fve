#include "../core/ast/ast.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

namespace {
Nodep sample() {
    auto t = var("t", Dim::none(), Ival{0.5, 2.0});
    auto inner = oper(Op::Mul, {konst(2.0), t});
    return oper(Op::Sub, {oper(Op::Div, {konst(10.0), inner}), konst(1.0)});
}
}

void test_ast() {
    cur = "ast";
    auto a = sample();
    auto b = sample();
    CHECK(a->hash == b->hash);

    std::string s = serialize(a);
    auto c = deserialize(s);
    CHECK(a->hash == c->hash);
    CHECK(serialize(c) == s);

    auto d = oper(Op::Add, {konst(1.0), konst(2.0)});
    auto e = oper(Op::Add, {konst(2.0), konst(1.0)});
    CHECK(d->hash != e->hash);

    Op o;
    CHECK(op_from_name("sqrt", o) && o == Op::Sqrt);
    CHECK(!op_from_name("nope", o));
    CHECK(op_name(Op::Div) == "div");
    CHECK(op_arity(Op::Sqrt) == 1);
    CHECK(op_arity(Op::Add) == 2);

    auto lin = linearize(a);
    CHECK(lin.size() == 7);
    CHECK(lin.back().kind == Kind::Oper && lin.back().op == Op::Sub);

    auto v = var("x");
    auto reuse = oper(Op::Add, {v, v});
    auto rl = linearize(reuse);
    CHECK(rl.size() == 2);

    auto h = hole("h1", 2, Ival{0.0, 5.0}, 'B', {konst(1.0), var("y")});
    auto hj = deserialize(serialize(h));
    CHECK(h->hash == hj->hash);
    CHECK(hj->kind == Kind::Hole && hj->regime == 'B');

    auto dj = disj({konst(3.0), konst(8.0)});
    auto dj2 = deserialize(serialize(dj));
    CHECK(dj->hash == dj2->hash);
    CHECK(dj2->cands.size() == 2);
}
