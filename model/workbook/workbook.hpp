#pragma once
#include <map>
#include <string>
#include <vector>

namespace fve {

struct Ref {
    int sheet = 0;
    int row = 0;
    int col = 0;
    bool abs_row = false;
    bool abs_col = false;
};

bool operator<(const Ref& a, const Ref& b);
bool operator==(const Ref& a, const Ref& b);

std::string col_name(int col);
int col_index(const std::string& s);
std::string a1(int row, int col);
bool parse_a1(const std::string& s, int& row, int& col, bool& ar, bool& ac);

struct Cell {
    bool has_value = false;
    double value = 0.0;
    std::string text;
    std::string formula;
    bool is_input = false;
};

struct Sheet {
    std::string name;
    std::map<std::pair<int, int>, Cell> cells;
    Cell* at(int row, int col);
    const Cell* at(int row, int col) const;
    void set(int row, int col, const Cell& c);
};

struct Name {
    std::string id;
    Ref target;
    int sheet = -1;
    bool is_range = false;
    Ref end;
};

struct Workbook {
    std::vector<Sheet> sheets;
    std::map<std::string, Name> names;
    int sheet_index(const std::string& name) const;
    Sheet& sheet(const std::string& name);
    std::string key(const Ref& r) const;
    std::string key(int sheet, int row, int col) const;
};

std::string ref_str(const Workbook& wb, const Ref& r);

}
