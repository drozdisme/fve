#pragma once
#include "../extractor/formula/formula.hpp"
#include "../model/workbook/workbook.hpp"
#include <map>
#include <vector>

namespace fve {

struct CellId {
    int sheet, row, col;
};
bool operator<(const CellId& a, const CellId& b);

class Oracle {
public:
    explicit Oracle(Workbook& wb);
    void build();
    double value(int sheet, int row, int col);
    void set(int sheet, int row, int col, double v);
    void reset();
    std::vector<CellId> inputs() const { return inputs_; }
    std::vector<CellId> outputs() const { return outputs_; }
    bool resolve_name(const std::string& nm, Ref& out) const;

private:
    double eval(const XAstp& a, int cur_sheet, int depth);
    double cell_val(const Ref& r, int cur_sheet, int depth);
    void range_vals(const XAstp& a, int cur_sheet, std::vector<double>& out, int depth);

    Workbook& wb_;
    std::map<CellId, double> val_;
    std::map<CellId, XAstp> fx_;
    std::map<CellId, double> base_;
    std::vector<CellId> inputs_;
    std::vector<CellId> outputs_;
};

}
