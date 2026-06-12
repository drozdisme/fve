#include "../extractor/pipeline.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_pipeline() {
    cur = "pipeline";
    Extracted ex = extract_file("tests/fixtures/beam.xlsx");
    CHECK(ex.ok);
    CHECK(ex.fmt == "xlsx");
    CHECK(ex.dep.edges.size() == 6);

    Oracle orc(ex.wb);
    orc.build();
    AuditChain chain;
    auto targets = margin_targets(ex.sdg);
    CHECK(targets.size() == 2);

    int safe = 0;
    for (const auto& t : targets) {
        Verified v = verify_target(ex.wb, orc, ex.sdg, t, 0.05, chain);
        CHECK(v.ok);
        CHECK(validate(v.run.cert));
        if (v.run.verdict == Verdict::Safe) safe++;
    }
    CHECK(safe == 2);
    CHECK(chain.verify());

    Extracted hx = extract_file("tests/fixtures/holed.xlsx");
    CHECK(hx.ok);
    Oracle ho(hx.wb);
    ho.build();
    AuditChain hc;
    bool any_hole = false, any_disj = false;
    for (const auto& t : margin_targets(hx.sdg)) {
        Verified v = verify_target(hx.wb, ho, hx.sdg, t, 0.05, hc);
        if (!v.ok) continue;
        if (v.lowered.holes > 0) any_hole = true;
        std::vector<Slot> lin = linearize(v.lowered.root);
        for (auto& s : lin) if (s.kind == Kind::Disj) any_disj = true;
        CHECK(validate(v.run.cert));
    }
    CHECK(any_hole);
    CHECK(any_disj);

    Extracted bx = extract_file("tests/fixtures/mini.xlsb");
    CHECK(bx.ok);
    CHECK(bx.fmt == "xlsb");
    CHECK(bx.wb.sheets.size() == 1);
}
