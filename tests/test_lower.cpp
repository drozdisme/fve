#include "../model/lower/lower.hpp"
#include "framework.hpp"
#include <set>

using namespace fve;
using namespace fvetest;

namespace {
bool has_kind(const Nodep& n, Kind k) {
    std::set<std::string> seen;
    std::vector<Nodep> st{n};
    while (!st.empty()) {
        Nodep c = st.back();
        st.pop_back();
        if (c->kind == k) return true;
        for (auto& a : c->args) st.push_back(a);
        for (auto& a : c->cands) st.push_back(a);
    }
    return false;
}
bool has_op(const Nodep& n, Op o) {
    std::vector<Nodep> st{n};
    while (!st.empty()) {
        Nodep c = st.back();
        st.pop_back();
        if (c->kind == Kind::Oper && c->op == o) return true;
        for (auto& a : c->args) st.push_back(a);
        for (auto& a : c->cands) st.push_back(a);
    }
    return false;
}
Nodep lower_one(const std::string& formula) {
    Workbook wb;
    Sheet& s = wb.sheet("S");
    Cell in;
    in.has_value = true;
    in.value = 4.0;
    s.set(0, 0, in);
    s.set(0, 1, in);
    s.set(0, 2, in);
    Cell f;
    f.formula = formula;
    s.set(5, 0, f);
    Oracle orc(wb);
    orc.build();
    Lowered lw = lower_target(wb, orc, {0, 5, 0});
    return lw.root;
}
}

void test_lower() {
    cur = "lower";
    CHECK(has_op(lower_one("=A1+B1"), Op::Add));
    CHECK(has_op(lower_one("=A1-B1"), Op::Sub));
    CHECK(has_op(lower_one("=A1*B1"), Op::Mul));
    CHECK(has_op(lower_one("=A1/B1"), Op::Div));
    CHECK(has_op(lower_one("=A1^2"), Op::Pow));
    CHECK(has_op(lower_one("=-A1"), Op::Neg));
    CHECK(has_op(lower_one("=A1%"), Op::Mul));
    CHECK(has_op(lower_one("=SQRT(A1)"), Op::Sqrt));
    CHECK(has_op(lower_one("=EXP(A1)"), Op::Exp));
    CHECK(has_op(lower_one("=LN(A1)"), Op::Log));
    CHECK(has_op(lower_one("=LOG(A1)"), Op::Div));
    CHECK(has_op(lower_one("=LOG(A1,2)"), Op::Div));
    CHECK(has_op(lower_one("=SIN(A1)"), Op::Sin));
    CHECK(has_op(lower_one("=COS(A1)"), Op::Cos));
    CHECK(has_op(lower_one("=TAN(A1)"), Op::Tan));
    CHECK(has_op(lower_one("=POWER(A1,3)"), Op::Pow));
    CHECK(has_op(lower_one("=SUM(A1:C1)"), Op::Add));
    CHECK(has_op(lower_one("=AVERAGE(A1:C1)"), Op::Div));

    CHECK(has_kind(lower_one("=PI()"), Kind::Const));
    CHECK(has_kind(lower_one("=MIN(A1,B1)"), Kind::Call));
    CHECK(has_kind(lower_one("=MAX(A1,B1)"), Kind::Call));
    CHECK(has_kind(lower_one("=ABS(A1)"), Kind::Call));
    CHECK(has_kind(lower_one("=IF(A1>0,A1,B1)"), Kind::Disj));
    CHECK(has_kind(lower_one("=VLOOKUP(A1,B1:C1,2)"), Kind::Hole));
    CHECK(has_kind(lower_one("=A1&B1"), Kind::Hole));

    Workbook wb;
    Sheet& s = wb.sheet("S");
    Cell in;
    in.has_value = true;
    in.value = 10.0;
    s.set(0, 0, in);
    Cell mid;
    mid.formula = "=A1*2";
    s.set(1, 0, mid);
    Cell top;
    top.formula = "=B2+A2";
    s.set(0, 1, top);
    Oracle orc(wb);
    orc.build();
    Lowered lw = lower_target(wb, orc, {0, 0, 1});
    CHECK(lw.ok);
    CHECK(lw.input_base.size() == 1);
    CHECK(has_op(lw.root, Op::Add));
}
