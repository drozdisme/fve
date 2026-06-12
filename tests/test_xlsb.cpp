#include "../extractor/xlsb/xlsb.hpp"
#include "framework.hpp"
#include <cstring>

using namespace fve;
using namespace fvetest;

void test_xlsb() {
    cur = "xlsb";
    NEAR(rk_num((100 << 2) | 2), 100.0, 1e-9);
    NEAR(rk_num((250 << 2) | 2 | 1), 2.5, 1e-9);

    std::vector<uint8_t> bin;
    auto put = [&](uint8_t b) { bin.push_back(b); };
    put(0x05);
    put(16);
    uint32_t col = 0;
    for (int i = 0; i < 4; i++) put((col >> (i * 8)) & 0xFF);
    for (int i = 0; i < 4; i++) put(0);
    double d = 3.5;
    uint8_t* dp = (uint8_t*)&d;
    for (int i = 0; i < 8; i++) put(dp[i]);
    auto recs = read_records(bin);
    CHECK(recs.size() == 1);
    CHECK(recs[0].id == 0x05 && recs[0].len == 16);

    uint8_t rpn[16];
    int k = 0;
    rpn[k++] = 0x24;
    uint32_t row = 0;
    std::memcpy(rpn + k, &row, 4); k += 4;
    uint16_t c0 = 0;
    std::memcpy(rpn + k, &c0, 2); k += 2;
    rpn[k++] = 0x24;
    row = 1;
    std::memcpy(rpn + k, &row, 4); k += 4;
    uint16_t c1 = 0;
    std::memcpy(rpn + k, &c1, 2); k += 2;
    rpn[k++] = 0x03;
    bool ok = false;
    std::string f = decode_rpn(rpn, k, 0, 0, ok);
    CHECK(ok);
    CHECK(f == "(A1+A2)");

    LoadResult r = load_xlsb_file("tests/fixtures/mini.xlsb");
    CHECK(r.ok && r.fmt == "xlsb");
    CHECK(r.wb.sheets.size() == 1);
    Sheet& s = r.wb.sheets[0];
    const Cell* a1 = s.at(0, 0);
    const Cell* b1 = s.at(0, 1);
    const Cell* a2 = s.at(1, 0);
    CHECK(a1 && a1->value == 10.0);
    CHECK(b1 && b1->value == 32.0);
    CHECK(a2 && a2->value == 42.0);
    CHECK(a2 && a2->formula == "=(A1+B1)");

    uint8_t num[10];
    num[0] = 0x1E;
    uint16_t iv = 7;
    std::memcpy(num + 1, &iv, 2);
    num[3] = 0x1F;
    double dv = 2.5;
    std::memcpy(num + 4, &dv, 8 > 6 ? 6 : 8);
    bool ok2 = false;
    decode_rpn(num, 3, 0, 0, ok2);
    CHECK(ok2);

    uint8_t mulr[14];
    int j = 0;
    mulr[j++] = 0x1E;
    uint16_t two = 2;
    std::memcpy(mulr + j, &two, 2); j += 2;
    mulr[j++] = 0x1E;
    uint16_t three = 3;
    std::memcpy(mulr + j, &three, 2); j += 2;
    mulr[j++] = 0x05;
    bool ok3 = false;
    std::string fm = decode_rpn(mulr, j, 0, 0, ok3);
    CHECK(ok3 && fm == "(2*3)");

    uint8_t bad[2] = {0x7A, 0x00};
    bool ok4 = true;
    decode_rpn(bad, 1, 0, 0, ok4);
    CHECK(!ok4);

    NEAR(rk_num((5 << 2) | 2), 5.0, 1e-9);
}
