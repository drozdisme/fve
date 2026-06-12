#pragma once
#include "../../model/workbook/workbook.hpp"
#include <memory>
#include <string>
#include <vector>

namespace fve {

enum class XKind { Num, Str, Cell, Range, NameRef, Call, Bin, Un };

struct XAst;
using XAstp = std::shared_ptr<XAst>;

struct XAst {
    XKind kind;
    double num = 0.0;
    std::string str;
    std::string op;
    std::string fn;
    std::string name;
    Ref ref;
    Ref ref_end;
    std::string sheet;
    std::vector<XAstp> args;
};

struct XParse {
    XAstp ast;
    bool ok = false;
    std::string err;
};

XParse parse_formula(const std::string& src);

void collect_refs(const XAstp& a, std::vector<Ref>& cells,
                  std::vector<std::pair<Ref, Ref>>& ranges,
                  std::vector<std::string>& names);

std::string xast_str(const XAstp& a);

}
