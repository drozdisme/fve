#include "../extractor/zip/zip.hpp"
#include "framework.hpp"

using namespace fve;
using namespace fvetest;

void test_zip() {
    cur = "zip";
    Zip z = read_zip_file("tests/fixtures/beam.xlsx");
    CHECK(z.ok);
    CHECK(z.entries.size() >= 8);
    CHECK(z.has("xl/workbook.xml"));
    CHECK(z.has("[Content_Types].xml"));

    std::string wbx = z.text("xl/workbook.xml");
    CHECK(wbx.find("<workbook") != std::string::npos || wbx.find(":workbook") != std::string::npos);
    CHECK(wbx.find("Beam") != std::string::npos);

    std::string s1 = z.text("xl/worksheets/sheet1.xml");
    CHECK(s1.find("B1/(B2*B3)") != std::string::npos);

    Zip bad = read_zip(std::vector<uint8_t>{1, 2, 3});
    CHECK(!bad.ok || bad.entries.empty());

    Zip xb = read_zip_file("tests/fixtures/mini.xlsb");
    CHECK(xb.ok);
    CHECK(xb.has("xl/workbook.bin"));
    CHECK(xb.bytes("xl/workbook.bin")->size() > 0);
}
