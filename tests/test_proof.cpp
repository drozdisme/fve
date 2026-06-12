#include "../core/proof/proof.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

namespace {
Program prog(Ival L) {
    Program p;
    auto ms = oper(Op::Sub,
                   {oper(Op::Div, {var("sall"), oper(Op::Mul, {var("c"), var("L")})}),
                    konst(1.0)});
    p.root = ms;
    p.box = {{"sall", Ival::point(300)}, {"c", Ival::point(1)}, {"L", L}};
    return p;
}
}

void test_proof() {
    cur = "proof";
    Program ps = prog(Ival{90, 110});
    EvalResult rs = eval(ps);
    Certificate cs = build_cert(ps, rs, "interval");
    CHECK(cs.verdict == Verdict::Safe);
    CHECK(validate(cs));
    CHECK(cs.program_hash == ps.root->hash);

    Program pf = prog(Ival{400, 500});
    Certificate cf = build_cert(pf, eval(pf), "interval");
    CHECK(cf.verdict == Verdict::Fail);
    CHECK(validate(cf));

    Program pa = prog(Ival{200, 400});
    Certificate ca = build_cert(pa, eval(pa), "interval");
    CHECK(ca.verdict == Verdict::Abstain);
    CHECK(validate(ca));

    Certificate roundtrip = cert_from_json(cert_json(cs));
    CHECK(roundtrip.digest == cs.digest);
    CHECK(roundtrip.verdict == cs.verdict);

    Certificate tampered = cs;
    tampered.ms_lo = -5.0;
    CHECK(!validate(tampered));

    Certificate forged;
    forged.ms_lo = -1.0;
    forged.ms_hi = 1.0;
    forged.residual = 0.0;
    forged.verdict = Verdict::Safe;
    forged.method = "interval";
    forged.digest = cert_hash(forged);
    CHECK(!validate(forged));

    Certificate forged2;
    forged2.ms_lo = 0.5;
    forged2.ms_hi = 1.0;
    forged2.residual = 0.8;
    forged2.verdict = Verdict::Safe;
    forged2.digest = cert_hash(forged2);
    CHECK(!validate(forged2));

    Certificate forged_fail;
    forged_fail.ms_lo = -0.5;
    forged_fail.ms_hi = 0.5;
    forged_fail.residual = 0.0;
    forged_fail.verdict = Verdict::Fail;
    forged_fail.digest = cert_hash(forged_fail);
    CHECK(!validate(forged_fail));

    Certificate honest_abstain;
    honest_abstain.ms_lo = -0.5;
    honest_abstain.ms_hi = 0.5;
    honest_abstain.residual = 0.0;
    honest_abstain.verdict = Verdict::Abstain;
    honest_abstain.digest = cert_hash(honest_abstain);
    CHECK(validate(honest_abstain));

    Certificate neg_res;
    neg_res.ms_lo = 0.1;
    neg_res.ms_hi = 0.5;
    neg_res.residual = -1.0;
    neg_res.verdict = Verdict::Safe;
    neg_res.digest = cert_hash(neg_res);
    CHECK(!validate(neg_res));

    std::string s = cert_dump(cs);
    CHECK(s.find("SAFE") != std::string::npos);
}
