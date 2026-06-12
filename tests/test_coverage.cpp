#include "../platform/coverage.hpp"
#include "../model/workbook/workbook.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_coverage() {
    cur = "coverage";
    Workbook wb;
    Sheet sh;
    sh.name = "S";
    auto put = [&](int r, int c, const Cell& cell) { sh.cells[{r, c}] = cell; };

    Cell k; k.has_value = true; k.value = 1.0; put(0, 0, k);                         // constant
    Cell f; f.formula = "A1+1"; put(0, 1, f);                                        // covered formula
    Cell v; v.formula = "VLOOKUP(A1,B1:C9,2)"; put(0, 2, v);                         // unsupported
    Cell e; e.formula = "[1]Sheet1!A1+2"; put(0, 3, e);                              // external ref
    Cell t; t.text = "label"; put(0, 4, t);                                          // text only
    wb.sheets.push_back(sh);

    CoverageLedger cl = compute_coverage(wb);
    CHECK(cl.total_cells == 5);
    CHECK(cl.formula_cells == 3);
    CHECK(cl.constant_cells == 1);
    CHECK(cl.hole_cells >= 2); // VLOOKUP + external_ref (parse_error possibly also)
    CHECK(cl.hole_reasons.count("unsupported_function:VLOOKUP") == 1);
    CHECK(cl.hole_reasons.count("external_ref") == 1);
    CHECK(!cl.samples.empty());
    // confidence reflects the high uncovered ratio (2-3 of 3 formula cells)
    CHECK(cl.confidence == "low");

    // a clean workbook -> high confidence, no holes
    Workbook wb2;
    Sheet s2; s2.name = "S";
    Cell c1; c1.has_value = true; c1.value = 2.0; s2.cells[{0, 0}] = c1;
    Cell c2; c2.formula = "A1*2"; s2.cells[{0, 1}] = c2;
    wb2.sheets.push_back(s2);
    CoverageLedger cl2 = compute_coverage(wb2);
    CHECK(cl2.hole_cells == 0);
    CHECK(cl2.confidence == "high");
}
