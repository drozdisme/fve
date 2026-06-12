#include "../extractor/xlsx/xlsx.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_xlsx() {
    cur = "xlsx";
    LoadResult r = load_xlsx_file("tests/fixtures/beam.xlsx");
    CHECK(r.ok);
    CHECK(r.fmt == "xlsx");
    CHECK(r.wb.sheets.size() == 2);
    CHECK(r.wb.sheet_index("Beam") == 0);
    CHECK(r.wb.sheet_index("Data") == 1);

    Sheet& b = r.wb.sheet("Beam");
    const Cell* b1 = b.at(0, 1);
    CHECK(b1 && b1->has_value);
    NEAR(b1->value, 300.0, 1e-9);

    const Cell* a4 = b.at(3, 0);
    CHECK(a4 && a4->text == "MS");
    const Cell* b4 = b.at(3, 1);
    CHECK(b4 && b4->formula == "=B1/(B2*B3)-1");
    const Cell* b5 = b.at(4, 1);
    CHECK(b5 && b5->formula == "=SUM(B1:B3)");

    CHECK(r.wb.names.count("Allow") == 1);
    CHECK(r.wb.names["Allow"].target.row == 0 && r.wb.names["Allow"].target.col == 1);

    LoadResult m = load_xlsx_file("tests/fixtures/macro.xlsm");
    CHECK(m.ok);
    CHECK(m.fmt == "xlsm");

    LoadResult bad = load_xlsx_file("tests/fixtures/does_not_exist.xlsx");
    CHECK(!bad.ok);
}
