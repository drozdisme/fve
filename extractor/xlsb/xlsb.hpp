#pragma once
#include "../../model/workbook/workbook.hpp"
#include "../xlsx/xlsx.hpp"
#include "../zip/zip.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace fve {

struct Rec {
    int id;
    const uint8_t* data;
    size_t len;
};

std::vector<Rec> read_records(const std::vector<uint8_t>& bin);
double rk_num(uint32_t v);
std::string decode_rpn(const uint8_t* p, size_t n, int base_row, int base_col, bool& ok);

LoadResult load_xlsb(const Zip& z);
LoadResult load_xlsb_file(const std::string& path);

struct XlsbStats {
    uint64_t records = 0;
    uint64_t cells = 0;
    uint64_t rows = 0;
    long max_row = -1;
    long max_col = -1;
    uint64_t bytes = 0;
    int sheets = 0;
};

void stream_part_records(const ZipIndex& z, const std::string& part,
                         const std::function<void(uint32_t, const uint8_t*, size_t)>& on_rec);
XlsbStats stream_workbook_stats(const std::string& path);

}
