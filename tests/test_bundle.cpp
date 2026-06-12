#include "../model/bundle/bundle.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_bundle() {
    cur = "bundle";
    Bundle b;
    b.add(konst(1.0), 0.6, "a");
    b.add(konst(2.0), 0.3, "b");
    b.add(konst(3.0), 0.1, "c");
    b.normalize();
    NEAR(b.top_conf(), 0.6, 1e-9);
    CHECK(b.ambiguous());

    Nodep n = b.lower("h0");
    CHECK(n->kind == Kind::Disj);
    CHECK(n->cands.size() == 3);

    Bundle b2;
    b2.add(konst(5.0), 1.0, "only");
    CHECK(!b2.ambiguous());
    CHECK(b2.lower("h1")->kind == Kind::Const);

    Bundle b3;
    b3.add(konst(1.0), 0.95, "x");
    b3.add(konst(2.0), 0.04, "y");
    b3.add(konst(3.0), 0.01, "z");
    b3.normalize();
    b3.prune(0.1);
    CHECK(b3.size() == 2);

    Bundle b4;
    b4.mark_unknown(Ival{0.0, 10.0});
    Nodep u = b4.lower("h2");
    CHECK(u->kind == Kind::Hole);

    Bundle b5;
    b5.add(konst(1.0), 0.7, "k");
    b5.mark_unknown(Ival::entire());
    Nodep m = b5.lower("h3");
    CHECK(m->kind == Kind::Disj);
    bool has_hole = false;
    for (auto& c : m->cands) if (c->kind == Kind::Hole) has_hole = true;
    CHECK(has_hole);
}
