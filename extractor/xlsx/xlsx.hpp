#pragma once
#include "../../model/workbook/workbook.hpp"
#include "../zip/zip.hpp"
#include <string>

namespace fve {

struct LoadResult {
    Workbook wb;
    bool ok = false;
    std::string fmt;
    std::string err;
};

LoadResult load_xlsx(const Zip& z);
LoadResult load_xlsx_file(const std::string& path);

}
