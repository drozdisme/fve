#pragma once
#include "../../core/ast/ast.hpp"
#include "../../oracle/oracle.hpp"
#include "../../model/workbook/workbook.hpp"
#include <map>
#include <string>

namespace fve {

struct Lowered {
    Nodep root;
    std::map<std::string, double> input_base;
    int holes = 0;
    bool ok = false;
};

Lowered lower_target(Workbook& wb, Oracle& orc, const CellId& target);

}
