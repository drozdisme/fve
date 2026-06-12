#include "xlsx.hpp"
#include "../xml/xml.hpp"
#include <algorithm>
#include <cstdlib>

namespace fve {

namespace {

std::vector<std::string> shared_strings(const Zip& z) {
    std::vector<std::string> out;
    std::string xml = z.text("xl/sharedStrings.xml");
    if (xml.empty()) return out;
    Xmlp root = parse_xml(xml);
    if (!root) return out;
    for (const auto& si : root->all("si")) {
        std::string s;
        Xmlp t = si->first("t");
        if (t) s = t->text;
        for (const auto& r : si->all("r")) {
            Xmlp rt = r->first("t");
            if (rt) s += rt->text;
        }
        out.push_back(s);
    }
    return out;
}

std::map<std::string, std::string> rels(const Zip& z) {
    std::map<std::string, std::string> m;
    std::string xml = z.text("xl/_rels/workbook.xml.rels");
    Xmlp root = parse_xml(xml);
    if (!root) return m;
    for (const auto& r : root->all("Relationship")) {
        std::string tgt = r->attr("Target");
        if (!tgt.empty() && tgt[0] != '/' && tgt.compare(0, 3, "xl/") != 0)
            tgt = "xl/" + tgt;
        m[r->attr("Id")] = tgt;
    }
    return m;
}

void parse_sheet(const std::string& xml, Sheet& sh, int sheet_idx,
                 const std::vector<std::string>& sst) {
    Xmlp root = parse_xml(xml);
    if (!root) return;
    Xmlp data = root->first("sheetData");
    if (!data) return;
    for (const auto& row : data->all("row")) {
        for (const auto& c : row->all("c")) {
            std::string rstr = c->attr("r");
            int rr, cc;
            bool ar, ac;
            if (!parse_a1(rstr, rr, cc, ar, ac)) continue;
            std::string type = c->attr("t", "n");
            Cell cell;
            Xmlp f = c->first("f");
            if (f && !f->text.empty()) cell.formula = "=" + f->text;
            Xmlp v = c->first("v");
            if (type == "s") {
                if (v) {
                    int idx = atoi(v->text.c_str());
                    if (idx >= 0 && idx < (int)sst.size()) {
                        cell.text = sst[idx];
                        cell.has_value = true;
                    }
                }
            } else if (type == "inlineStr") {
                Xmlp is = c->first("is");
                if (is) {
                    Xmlp t = is->first("t");
                    if (t) { cell.text = t->text; cell.has_value = true; }
                }
            } else if (type == "str") {
                if (v) { cell.text = v->text; cell.has_value = true; }
            } else {
                if (v) {
                    cell.value = strtod(v->text.c_str(), nullptr);
                    cell.has_value = true;
                }
            }
            (void)sheet_idx;
            sh.set(rr, cc, cell);
        }
    }
}

void parse_names(const Xmlp& wbroot, Workbook& wb) {
    Xmlp dn = wbroot->first("definedNames");
    if (!dn) return;
    for (const auto& d : dn->all("definedName")) {
        Name nm;
        nm.id = d->attr("name");
        std::string body = d->text;
        size_t bang = body.rfind('!');
        std::string sheet, cellpart = body;
        if (bang != std::string::npos) {
            sheet = body.substr(0, bang);
            if (!sheet.empty() && sheet.front() == '\'' && sheet.back() == '\'')
                sheet = sheet.substr(1, sheet.size() - 2);
            cellpart = body.substr(bang + 1);
        }
        nm.sheet = wb.sheet_index(sheet);
        size_t colon = cellpart.find(':');
        int r, c;
        bool ar, ac;
        if (colon != std::string::npos) {
            nm.is_range = true;
            parse_a1(cellpart.substr(0, colon), nm.target.row, nm.target.col, ar, ac);
            if (parse_a1(cellpart.substr(colon + 1), r, c, ar, ac)) {
                nm.end.row = r;
                nm.end.col = c;
            }
        } else {
            parse_a1(cellpart, nm.target.row, nm.target.col, ar, ac);
        }
        nm.target.sheet = nm.sheet;
        if (!nm.id.empty()) wb.names[nm.id] = nm;
    }
}

}

LoadResult load_xlsx(const Zip& z) {
    LoadResult res;
    if (!z.ok) { res.err = "bad zip"; return res; }
    std::string ct = z.text("[Content_Types].xml");
    res.fmt = ct.find("ms-excel.sheet.macroEnabled") != std::string::npos ? "xlsm" : "xlsx";

    std::string wbxml = z.text("xl/workbook.xml");
    Xmlp wbroot = parse_xml(wbxml);
    if (!wbroot) { res.err = "no workbook.xml"; return res; }
    auto rmap = rels(z);
    auto sst = shared_strings(z);

    Xmlp sheets = wbroot->first("sheets");
    std::vector<std::pair<std::string, std::string>> order;
    if (sheets) {
        for (const auto& s : sheets->all("sheet")) {
            std::string name = s->attr("name");
            std::string rid = s->attr("id");
            order.push_back({name, rid});
            res.wb.sheet(name);
        }
    }
    for (size_t i = 0; i < order.size(); i++) {
        std::string path = rmap.count(order[i].second) ? rmap[order[i].second]
                                                        : "xl/worksheets/sheet" + std::to_string(i + 1) + ".xml";
        std::string sx = z.text(path);
        if (sx.empty()) sx = z.text("xl/worksheets/sheet" + std::to_string(i + 1) + ".xml");
        parse_sheet(sx, res.wb.sheets[i], (int)i, sst);
    }
    parse_names(wbroot, res.wb);
    res.ok = true;
    return res;
}

LoadResult load_xlsx_file(const std::string& path) {
    Zip z = read_zip_file(path);
    return load_xlsx(z);
}

}
