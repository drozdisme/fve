#include "workbook.hpp"

namespace fve {

bool operator<(const Ref& a, const Ref& b) {
    if (a.sheet != b.sheet) return a.sheet < b.sheet;
    if (a.row != b.row) return a.row < b.row;
    return a.col < b.col;
}

bool operator==(const Ref& a, const Ref& b) {
    return a.sheet == b.sheet && a.row == b.row && a.col == b.col;
}

std::string col_name(int col) {
    std::string s;
    col += 1;
    while (col > 0) {
        int r = (col - 1) % 26;
        s = char('A' + r) + s;
        col = (col - 1) / 26;
    }
    return s;
}

int col_index(const std::string& s) {
    int c = 0;
    for (char ch : s) {
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        if (ch < 'A' || ch > 'Z') break;
        c = c * 26 + (ch - 'A' + 1);
    }
    return c - 1;
}

std::string a1(int row, int col) { return col_name(col) + std::to_string(row + 1); }

bool parse_a1(const std::string& s, int& row, int& col, bool& ar, bool& ac) {
    size_t i = 0;
    ac = false;
    ar = false;
    if (i < s.size() && s[i] == '$') { ac = true; i++; }
    std::string letters;
    while (i < s.size() && ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) {
        letters += s[i++];
    }
    if (letters.empty()) return false;
    if (i < s.size() && s[i] == '$') { ar = true; i++; }
    std::string digits;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') digits += s[i++];
    if (digits.empty()) return false;
    col = col_index(letters);
    row = std::stoi(digits) - 1;
    return i == s.size();
}

Cell* Sheet::at(int row, int col) {
    auto it = cells.find({row, col});
    return it == cells.end() ? nullptr : &it->second;
}

const Cell* Sheet::at(int row, int col) const {
    auto it = cells.find({row, col});
    return it == cells.end() ? nullptr : &it->second;
}

void Sheet::set(int row, int col, const Cell& c) { cells[{row, col}] = c; }

int Workbook::sheet_index(const std::string& name) const {
    for (size_t i = 0; i < sheets.size(); i++)
        if (sheets[i].name == name) return (int)i;
    return -1;
}

Sheet& Workbook::sheet(const std::string& name) {
    int i = sheet_index(name);
    if (i >= 0) return sheets[i];
    Sheet s;
    s.name = name;
    sheets.push_back(s);
    return sheets.back();
}

std::string Workbook::key(const Ref& r) const {
    std::string sn = (r.sheet >= 0 && r.sheet < (int)sheets.size())
                         ? sheets[r.sheet].name
                         : std::to_string(r.sheet);
    return sn + "!" + a1(r.row, r.col);
}

std::string ref_str(const Workbook& wb, const Ref& r) { return wb.key(r); }

std::string Workbook::key(int sheet, int row, int col) const {
    Ref r;
    r.sheet = sheet;
    r.row = row;
    r.col = col;
    return key(r);
}

}
