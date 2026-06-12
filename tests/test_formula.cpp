#include "../extractor/formula/formula.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

namespace {
XAstp pa(const char* s) { return parse_formula(s).ast; }
}

void test_formula() {
    cur = "formula";
    CHECK(parse_formula("=A1+B2*3").ok);
    CHECK(parse_formula("=SUM(A1:A10)/COUNT(A1:A10)").ok);
    CHECK(parse_formula("=IF(C3>0,SQRT(C3),0)").ok);
    CHECK(parse_formula("=(A1+A2)*(B1-B2)").ok);
    CHECK(parse_formula("=Sheet2!C4+'My Sheet'!D5").ok);
    CHECK(parse_formula("=VLOOKUP(A1,Data!A1:C10,3,FALSE)").ok);
    CHECK(parse_formula("=PI()*r^2").ok);
    CHECK(!parse_formula("=A1+").ok);
    CHECK(!parse_formula("=(A1+2").ok);
    CHECK(!parse_formula("").ok);

    XAstp a = pa("=A1+B2*3");
    CHECK(a->kind == XKind::Bin && a->op == "+");
    CHECK(a->args[1]->kind == XKind::Bin && a->args[1]->op == "*");

    XAstp c = pa("=SUM(A1:A5)");
    CHECK(c->kind == XKind::Call && c->fn == "SUM");
    CHECK(c->args[0]->kind == XKind::Range);
    CHECK(c->args[0]->ref.row == 0 && c->args[0]->ref_end.row == 4);

    XAstp cell = pa("=Data!B7");
    CHECK(cell->kind == XKind::Cell && cell->sheet == "Data");
    CHECK(cell->ref.row == 6 && cell->ref.col == 1);

    XAstp pct = pa("=50%");
    CHECK(pct->kind == XKind::Un && pct->op == "%");

    XAstp pw = pa("=2^3^2");
    CHECK(pw->kind == XKind::Bin && pw->op == "^");
    CHECK(pw->args[1]->kind == XKind::Bin);

    std::vector<Ref> cells;
    std::vector<std::pair<Ref, Ref>> ranges;
    std::vector<std::string> names;
    collect_refs(pa("=SUM(A1:A5)+B2*myname-C3"), cells, ranges, names);
    CHECK(cells.size() == 2);
    CHECK(ranges.size() == 1);
    CHECK(names.size() == 1 && names[0] == "myname");

    CHECK(col_index("A") == 0);
    CHECK(col_index("Z") == 25);
    CHECK(col_index("AA") == 26);
    CHECK(col_name(0) == "A");
    CHECK(col_name(27) == "AB");
    int r, co;
    bool ar, ac;
    CHECK(parse_a1("$C$7", r, co, ar, ac) && r == 6 && co == 2 && ar && ac);

    CHECK(parse_formula("=A1&B1").ok);
    CHECK(parse_formula("=A1<>B1").ok);
    CHECK(parse_formula("=A1>=B1").ok);
    CHECK(parse_formula("=+A1").ok);
    CHECK(parse_formula("=MAX(MIN(A1,B1),C1)").ok);
    CHECK(parse_formula("=3.5e-2+1").ok);
    CHECK(parse_formula("=\"hi\"&\"there\"").ok);
    CHECK(parse_formula("=AND(A1>0,B1<5)").ok);

    XAstp amp = pa("=A1&B1");
    CHECK(amp->kind == XKind::Bin && amp->op == "&");
    XAstp ne = pa("=A1<>B1");
    CHECK(ne->op == "<>");
    XAstp up = pa("=+A1");
    CHECK(up->kind == XKind::Un && up->op == "+");
    XAstp st = pa("=\"abc\"");
    CHECK(st->kind == XKind::Str && st->str == "abc");
    XAstp nm = pa("=myvar");
    CHECK(nm->kind == XKind::NameRef && nm->name == "myvar");

    CHECK(xast_str(pa("=A1+B1*2")).find("+") != std::string::npos);
}
