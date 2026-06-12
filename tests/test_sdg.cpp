#include "../extractor/xlsx/xlsx.hpp"
#include "../model/sdg/sdg.hpp"
#include "../oracle/intervene.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_sdg() {
    cur = "sdg";
    LoadResult lr = load_xlsx_file("tests/fixtures/beam.xlsx");
    Oracle orc(lr.wb);
    orc.build();
    DepGraph dep = induce_deps(orc);
    Sdg g = build_sdg(lr.wb, dep);

    CHECK(g.nodes.size() >= 5);
    CHECK(!g.edges.empty());

    int both = 0, syn = 0, iv = 0;
    for (const auto& e : g.edges) {
        if (e.origin == Origin::Both) both++;
        else if (e.origin == Origin::Syntactic) syn++;
        else iv++;
    }
    CHECK(both > 0);

    const SdgNode* ms = g.find({0, 3, 1});
    CHECK(ms && ms->label == "MS");
    const SdgNode* b1 = g.find({0, 0, 1});
    CHECK(b1 && b1->is_input);

    mark_ambiguous(g, {0, 3, 1}, 3);
    CHECK(g.find({0, 3, 1})->ambiguous);
    CHECK(g.find({0, 3, 1})->alts == 3);
    (void)syn;
    (void)iv;
}
