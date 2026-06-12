#include "../platform/incremental.hpp"
#include "../core/ast/ast.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_incremental() {
    cur = "incremental";

    // box fingerprint: same box -> same fp, different box -> different
    Box b1 = {{"x", Ival{0, 1}}, {"y", Ival{2, 3}}};
    Box b2 = {{"x", Ival{0, 1}}, {"y", Ival{2, 3}}};
    Box b3 = {{"x", Ival{0, 1.5}}, {"y", Ival{2, 3}}};
    CHECK(box_fingerprint(b1) == box_fingerprint(b2));
    CHECK(box_fingerprint(b1) != box_fingerprint(b3));

    EvalCache cache;
    EvalResult r;
    r.ms = Ival{0.2, 0.5};
    Certificate c;
    c.verdict = Verdict::Safe;
    CHECK(!cache.get("ph1", b1).has_value());
    cache.put("ph1", b1, r, c);
    auto hit = cache.get("ph1", b2); // same fingerprint
    CHECK(hit.has_value());
    CHECK(hit->cert.verdict == Verdict::Safe);
    CHECK(!cache.get("ph1", b3).has_value()); // different box
    CHECK(cache.size() == 1);

    // invalidate by program hash
    cache.put("ph2", b1, r, c);
    CHECK(cache.size() == 2);
    cache.invalidate("ph1");
    CHECK(cache.size() == 1);
    CHECK(!cache.get("ph1", b1).has_value());

    // invalidate by symbol
    cache.put("ph3", {{"L", Ival{90, 110}}}, r, c);
    CHECK(cache.size() == 2);
    cache.invalidate_by_symbol("L");
    CHECK(!cache.get("ph3", Box{{"L", Ival{90, 110}}}).has_value());

    // LRU eviction
    EvalCache lru;
    lru.set_limit(2);
    lru.put("a", b1, r, c);
    lru.put("b", b1, r, c);
    lru.get("a", b1);          // touch a -> b is now LRU
    lru.put("d", b1, r, c);    // evicts b
    CHECK(lru.size() == 2);
    CHECK(lru.get("a", b1).has_value());
    CHECK(!lru.get("b", b1).has_value());
    CHECK(lru.get("d", b1).has_value());

    // diff_programs: hash-consing
    Nodep oldr = oper(Op::Sub, {var("x"), konst(1.0)});
    Nodep newr = oper(Op::Sub, {var("x"), konst(2.0)});
    Nodep same = oper(Op::Sub, {var("x"), konst(1.0)});
    ProgramDiff d = diff_programs(oldr, newr);
    CHECK(d.structure_changed);
    CHECK(!d.changed_nodes.empty());          // konst(2) + new root
    CHECK(!d.unchanged_nodes.empty());        // var x shared
    ProgramDiff d2 = diff_programs(oldr, same);
    CHECK(!d2.structure_changed);
    CHECK(d2.changed_nodes.empty());

    // holes open/close
    Nodep ph = oper(Op::Add, {var("x"), hole("h1", 1, Ival{0, 1}, 'P', {var("z")})});
    Nodep noh = oper(Op::Add, {var("x"), var("z")});
    ProgramDiff dh = diff_programs(noh, ph);
    CHECK(dh.new_holes.size() == 1 && dh.new_holes[0] == "h1");
    ProgramDiff dh2 = diff_programs(ph, noh);
    CHECK(dh2.closed_holes.size() == 1);
}
