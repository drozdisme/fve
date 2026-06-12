#include "oracle.hpp"
#include <cmath>

namespace fve {

bool operator<(const CellId& a, const CellId& b) {
    if (a.sheet != b.sheet) return a.sheet < b.sheet;
    if (a.row != b.row) return a.row < b.row;
    return a.col < b.col;
}

Oracle::Oracle(Workbook& wb) : wb_(wb) {}

void Oracle::build() {
    val_.clear();
    fx_.clear();
    base_.clear();
    inputs_.clear();
    outputs_.clear();
    for (size_t si = 0; si < wb_.sheets.size(); si++) {
        for (auto& kv : wb_.sheets[si].cells) {
            int r = kv.first.first, c = kv.first.second;
            CellId id{(int)si, r, c};
            const Cell& cell = kv.second;
            if (!cell.formula.empty()) {
                XParse p = parse_formula(cell.formula);
                if (p.ok) {
                    fx_[id] = p.ast;
                    outputs_.push_back(id);
                }
            } else if (cell.has_value && cell.text.empty()) {
                val_[id] = cell.value;
                base_[id] = cell.value;
                inputs_.push_back(id);
            }
        }
    }
}

bool Oracle::resolve_name(const std::string& nm, Ref& out) const {
    auto it = wb_.names.find(nm);
    if (it == wb_.names.end()) return false;
    out = it->second.target;
    if (out.sheet < 0) out.sheet = 0;
    return true;
}

void Oracle::set(int sheet, int row, int col, double v) { val_[{sheet, row, col}] = v; }

void Oracle::reset() {
    val_.clear();
    for (auto& kv : base_) val_[kv.first] = kv.second;
}

double Oracle::value(int sheet, int row, int col) {
    CellId id{sheet, row, col};
    auto vit = val_.find(id);
    if (vit != val_.end()) return vit->second;
    auto fit = fx_.find(id);
    if (fit != fx_.end()) {
        double r = eval(fit->second, sheet, 0);
        val_[id] = r;
        return r;
    }
    return 0.0;
}

double Oracle::cell_val(const Ref& r, int cur_sheet, int depth) {
    int sh = r.sheet;
    if (sh < 0) sh = cur_sheet;
    CellId id{sh, r.row, r.col};
    auto vit = val_.find(id);
    if (vit != val_.end()) return vit->second;
    auto fit = fx_.find(id);
    if (fit != fx_.end()) {
        if (depth > 256) return std::nan("");
        double v = eval(fit->second, sh, depth + 1);
        val_[id] = v;
        return v;
    }
    return 0.0;
}

void Oracle::range_vals(const XAstp& a, int cur_sheet, std::vector<double>& out, int depth) {
    int sh = a->sheet.empty() ? cur_sheet : wb_.sheet_index(a->sheet);
    if (sh < 0) sh = cur_sheet;
    int r0 = std::min(a->ref.row, a->ref_end.row), r1 = std::max(a->ref.row, a->ref_end.row);
    int c0 = std::min(a->ref.col, a->ref_end.col), c1 = std::max(a->ref.col, a->ref_end.col);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
            Ref rr;
            rr.sheet = sh;
            rr.row = r;
            rr.col = c;
            out.push_back(cell_val(rr, cur_sheet, depth));
        }
}

double Oracle::eval(const XAstp& a, int cur_sheet, int depth) {
    if (!a) return std::nan("");
    switch (a->kind) {
        case XKind::Num: return a->num;
        case XKind::Str: return std::nan("");
        case XKind::Cell: {
            Ref r = a->ref;
            if (!a->sheet.empty()) r.sheet = wb_.sheet_index(a->sheet);
            else r.sheet = -1;
            return cell_val(r, cur_sheet, depth);
        }
        case XKind::Range: {
            std::vector<double> v;
            range_vals(a, cur_sheet, v, depth);
            return v.empty() ? std::nan("") : v[0];
        }
        case XKind::NameRef: {
            Ref r;
            if (resolve_name(a->name, r)) return cell_val(r, cur_sheet, depth);
            return std::nan("");
        }
        case XKind::Un: {
            double x = eval(a->args[0], cur_sheet, depth);
            if (a->op == "-") return -x;
            if (a->op == "%") return x / 100.0;
            return x;
        }
        case XKind::Bin: {
            double x = eval(a->args[0], cur_sheet, depth);
            double y = eval(a->args[1], cur_sheet, depth);
            const std::string& o = a->op;
            if (o == "+") return x + y;
            if (o == "-") return x - y;
            if (o == "*") return x * y;
            if (o == "/") return y == 0 ? std::nan("") : x / y;
            if (o == "^") return std::pow(x, y);
            if (o == "=") return x == y ? 1 : 0;
            if (o == "<>") return x != y ? 1 : 0;
            if (o == "<") return x < y ? 1 : 0;
            if (o == ">") return x > y ? 1 : 0;
            if (o == "<=") return x <= y ? 1 : 0;
            if (o == ">=") return x >= y ? 1 : 0;
            return std::nan("");
        }
        case XKind::Call: {
            const std::string& f = a->fn;
            std::vector<double> v;
            std::vector<std::vector<double>> av;
            for (const auto& arg : a->args) {
                std::vector<double> one;
                if (arg->kind == XKind::Range) range_vals(arg, cur_sheet, one, depth);
                else one.push_back(eval(arg, cur_sheet, depth));
                av.push_back(one);
                for (double x : one) v.push_back(x);
            }
            if (f == "SUM") { double s = 0; for (double x : v) if (!std::isnan(x)) s += x; return s; }
            if (f == "AVERAGE") { double s = 0; int n = 0; for (double x : v) if (!std::isnan(x)) { s += x; n++; } return n ? s / n : std::nan(""); }
            if (f == "MIN") { double m = 1e300; for (double x : v) if (!std::isnan(x)) m = std::min(m, x); return m; }
            if (f == "MAX") { double m = -1e300; for (double x : v) if (!std::isnan(x)) m = std::max(m, x); return m; }
            if (f == "COUNT") { int n = 0; for (double x : v) if (!std::isnan(x)) n++; return n; }
            if (f == "PRODUCT") { double p = 1; for (double x : v) if (!std::isnan(x)) p *= x; return p; }
            if (f == "SUMSQ") { double s = 0; for (double x : v) if (!std::isnan(x)) s += x * x; return s; }
            if (f == "SUMPRODUCT") {
                if (av.empty()) return 0;
                size_t n = av[0].size();
                double s = 0;
                for (size_t i = 0; i < n; i++) {
                    double p = 1;
                    for (auto& col : av) { if (i >= col.size()) { p = std::nan(""); break; } p *= col[i]; }
                    if (!std::isnan(p)) s += p;
                }
                return s;
            }
            if (f == "ABS") return std::fabs(v[0]);
            if (f == "SQRT") return std::sqrt(v[0]);
            if (f == "EXP") return std::exp(v[0]);
            if (f == "LN") return std::log(v[0]);
            if (f == "LOG") return v.size() > 1 ? std::log(v[0]) / std::log(v[1]) : std::log10(v[0]);
            if (f == "LOG10") return std::log10(v[0]);
            if (f == "SIN") return std::sin(v[0]);
            if (f == "COS") return std::cos(v[0]);
            if (f == "TAN") return std::tan(v[0]);
            if (f == "ASIN") return std::asin(v[0]);
            if (f == "ACOS") return std::acos(v[0]);
            if (f == "ATAN") return std::atan(v[0]);
            if (f == "ATAN2") return std::atan2(v[1], v[0]);
            if (f == "SINH") return std::sinh(v[0]);
            if (f == "COSH") return std::cosh(v[0]);
            if (f == "TANH") return std::tanh(v[0]);
            if (f == "DEGREES") return v[0] * 180.0 / 3.14159265358979311599796346854;
            if (f == "RADIANS") return v[0] * 3.14159265358979311599796346854 / 180.0;
            if (f == "POWER") return std::pow(v[0], v[1]);
            if (f == "PI") return 3.14159265358979311599796346854;
            if (f == "MOD") return v[1] == 0 ? std::nan("") : v[0] - v[1] * std::floor(v[0] / v[1]);
            if (f == "INT") return std::floor(v[0]);
            if (f == "TRUNC") { double m = std::pow(10, v.size() > 1 ? v[1] : 0); return std::trunc(v[0] * m) / m; }
            if (f == "SIGN") return v[0] > 0 ? 1 : (v[0] < 0 ? -1 : 0);
            if (f == "FLOOR") { double sig = v.size() > 1 ? v[1] : 1; return sig == 0 ? 0 : std::floor(v[0] / sig) * sig; }
            if (f == "CEILING") { double sig = v.size() > 1 ? v[1] : 1; return sig == 0 ? 0 : std::ceil(v[0] / sig) * sig; }
            if (f == "ROUNDUP") { double m = std::pow(10, v.size() > 1 ? v[1] : 0); return std::ceil(std::fabs(v[0]) * m) / m * (v[0] < 0 ? -1 : 1); }
            if (f == "ROUNDDOWN") { double m = std::pow(10, v.size() > 1 ? v[1] : 0); return std::floor(std::fabs(v[0]) * m) / m * (v[0] < 0 ? -1 : 1); }
            if (f == "IF") return v[0] != 0 ? v[1] : (v.size() > 2 ? v[2] : 0);
            if (f == "IFERROR") return std::isnan(v[0]) ? (v.size() > 1 ? v[1] : 0) : v[0];
            if (f == "AND") { for (double x : v) if (x == 0) return 0; return 1; }
            if (f == "OR") { for (double x : v) if (x != 0) return 1; return 0; }
            if (f == "NOT") return v[0] == 0 ? 1 : 0;
            if (f == "ROUND") { double p = v.size() > 1 ? v[1] : 0; double m = std::pow(10, p); return std::round(v[0] * m) / m; }
            return std::nan("");
        }
    }
    return std::nan("");
}

}
