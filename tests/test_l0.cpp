#include "../l0/crawler.hpp"
#include "../l0/version.hpp"
#include "../l0/fingerprint.hpp"
#include "../l0/registry.hpp"
#include "../db/store.hpp"
#include "../platform/service.hpp"
#include "../extractor/zip/zip.hpp"
#include "../extractor/xlsb/xlsb.hpp"
#include "../core/merkle/sha256.hpp"
#include "framework.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string l0td() {
    static int c = 0;
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_l0_" + std::to_string(getpid()) + "_" + std::to_string(ns) + "_" + std::to_string(c++);
}
const FileEntry* by_rel(const Catalog& cat, const std::string& rel) {
    for (const auto& e : cat.files) if (e.rel == rel) return &e;
    return nullptr;
}
}

void test_l0() {
    cur = "l0";

    Rev r1 = parse_revision("stringer_loads_Rev_D.xlsx");
    CHECK(r1.config_key == "stringer_loads" && r1.found);
    NEAR(r1.rank, 4.0, 1e-9);
    Rev r2 = parse_revision("stringer_loads_Rev_C.xlsx");
    NEAR(r2.rank, 3.0, 1e-9);
    CHECK(r2.config_key == r1.config_key);
    Rev r3 = parse_revision("bolt_joint_v10.xlsx");
    CHECK(r3.config_key == "bolt_joint");
    NEAR(r3.rank, 10.0, 1e-9);
    Rev r4 = parse_revision("frame_R3.csv");
    NEAR(r4.rank, 3.0, 1e-9);
    Rev r5 = parse_revision("rivet.xlsx");
    CHECK(!r5.found && r5.config_key == "rivet");
    CHECK(lower_ext("A.XLSX") == "xlsx");
    CHECK(lower_ext("noext") == "");

    CrawlResult cr = crawl("tests/fixtures/tree", {"xlsx", "xlsm", "xlsb", "csv"});
    CHECK(cr.dirs.size() == 10);
    CHECK(cr.files.size() == 8);
    CHECK(cr.dirs[0].parent == -1 && cr.dirs[0].depth == 0);
    bool found_deep = false;
    for (const auto& d : cr.dirs)
        if (d.rel == "fuselage/section_655/stringers") {
            CHECK(d.depth == 3);
            CHECK(cr.dirs[d.parent].rel == "fuselage/section_655");
            found_deep = true;
        }
    CHECK(found_deep);

    Catalog cat = build_registry("tests/fixtures/tree");
    CHECK(cat.folders.size() == 10);
    CHECK(cat.files.size() == 8);
    int heads = 0;
    for (const auto& e : cat.files) {
        CHECK(!e.sha256.empty());
        CHECK(!e.structural_sig.empty());
        if (e.is_head) heads++;
    }
    CHECK(heads == 5);

    const FileEntry* d = by_rel(cat, "fuselage/section_655/stringers/stringer_loads_Rev_D.xlsx");
    const FileEntry* c = by_rel(cat, "fuselage/section_655/stringers/stringer_loads_Rev_C.xlsx");
    CHECK(d && c);
    CHECK(d->is_head && !c->is_head);
    CHECK(d->format_type == "stringer_fem_loads");
    CHECK(d->domain == "structural.stringer");
    CHECK(d->config_key == c->config_key);
    CHECK(d->version_group == c->version_group);

    const FileEntry* rv2 = by_rel(cat, "fuselage/section_676/fasteners/rivet_joint_R2.xlsx");
    CHECK(rv2 && rv2->is_head && rv2->format_type == "rivet_joint");
    const FileEntry* panel = by_rel(cat, "wing/panels/panel_buckling.csv");
    CHECK(panel && panel->format_type == "panel_buckling");

    const FileEntry* hd = head_of(cat, "stringer_loads");
    CHECK(hd && hd->is_head);
    CHECK(hd == d);

    int stringers_folder = -1;
    for (const auto& f : cat.folders)
        if (f.rel == "fuselage/section_655/stringers") stringers_folder = f.id;
    CHECK(stringers_folder >= 0);
    CHECK(files_in(cat, stringers_folder).size() == 2);
    int fuselage = -1;
    for (const auto& f : cat.folders) if (f.rel == "fuselage") fuselage = f.id;
    CHECK(child_folders(cat, fuselage).size() == 2);

    std::string sql = registry_sql(cat);
    CHECK(sql.find("INSERT INTO folders") != std::string::npos);
    CHECK(sql.find("INSERT INTO file_registry") != std::string::npos);
    CHECK(sql.find("is_head") != std::string::npos);
    CHECK(sql.find("fuselage/section_655/stringers") != std::string::npos);

    std::string dir = l0td();
    {
        Store store(dir);
        migrate(cat, store);
        CHECK(store.list("folders").size() == 10);
        CHECK(store.list("file_registry").size() == 8);
        JsonP idx = store.get("l0", "registry_index");
        CHECK(idx && (int)idx->n("heads") == 5);
        JsonP fd = store.get("file_registry", d->id);
        CHECK(fd && fd->str("rel_path") == d->rel);
        CHECK(fd->at("is_head")->b);
        CHECK((int)fd->n("folder_id") == d->folder);
    }

    Fingerprint ft = fingerprint_text("PANEL,BUCKLING,sigma_cr,kc\n1,yes,250,4", "csv");
    CHECK(ft.format_type == "panel_buckling");
    CHECK(!schema_registry().empty());

    Rev g1 = parse_revision("777P2F_SnS_analysis_template_RevG1_655_676.xlsb");
    CHECK(g1.found && g1.config_key == "777p2f_sns_analysis_template_655_676");
    NEAR(g1.rank, 7.001, 1e-6);
    Rev gg = parse_revision("777P2F_SnS_analysis_template_RevG_655_676.xlsb");
    Rev gh = parse_revision("777P2F_SnS_analysis_template_RevH_655_676.xlsb");
    CHECK(gh.rank > g1.rank && g1.rank > gg.rank);
    CHECK(gg.config_key == g1.config_key && gh.config_key == g1.config_key);
    Rev nrev = parse_revision("skin_analysis_Revised_final.xlsx");
    CHECK(!nrev.found);

    std::string fx = "tests/fixtures/tree/fuselage/section_655/stringers/stringer_loads_Rev_D.xlsx";
    std::ifstream sf(fx, std::ios::binary);
    std::stringstream sb;
    sb << sf.rdbuf();
    std::string sc = sb.str();
    std::vector<uint8_t> sbytes(sc.begin(), sc.end());
    CHECK(hex(sha256(sbytes)) == hex(sha256_file(fx)));

    ZipIndex zi = open_zip_file(fx);
    CHECK(zi.ok);
    CHECK(!zi.names().empty());
    CHECK(zi.has("xl/workbook.xml"));
    CHECK(!zi.extract("xl/workbook.xml").empty());
    CHECK(zi.extract("xl/workbook.xml", 1).empty());
    CHECK(zi.extract("does/not/exist.bin").empty());

    std::string sdir = l0td();
    Service svc(sdir);
    JsonP cw = svc.crawl("tests/fixtures/tree");
    CHECK(cw->at("ok")->b && (int)cw->n("heads") == 5);
    JsonP s1 = svc.search("stringer");
    CHECK((int)s1->n("count") >= 1);
    JsonP newest = s1->at("newest");
    CHECK(newest && newest->at("is_head")->b);
    CHECK(newest->str("revision") == "revd");
    CHECK(newest->str("rel_path").find("Rev_D") != std::string::npos);
    JsonP s2 = svc.search("rivet");
    CHECK(s2->at("newest") && s2->at("newest")->str("revision") == "r2");
    JsonP s3 = svc.search("nonexistent_xyz");
    CHECK((int)s3->n("count") == 0);

    std::string cyc = "/tmp/fve_cyc_" + std::to_string(getpid());
    std::string cmd = "rm -rf " + cyc + "; mkdir -p " + cyc + "/a/b; cp " + fx + " " + cyc +
                      "/a/d.xlsx; ln -s " + cyc + "/a " + cyc + "/a/b/loop";
    int rc = std::system(cmd.c_str());
    (void)rc;
    CrawlResult cc = crawl(cyc, {"xlsx"});
    bool cycle_caught = false;
    for (const auto& er : cc.errors)
        if (er.reason == "cycle_skipped") cycle_caught = true;
    CHECK(cycle_caught);
    CHECK(cc.files.size() == 1);
    std::system(("rm -rf " + cyc).c_str());

    XlsbStats xs = stream_workbook_stats("tests/fixtures/mini.xlsb");
    CHECK(xs.sheets >= 1);
    CHECK(xs.cells >= 1);
    Zip fz = read_zip_file("tests/fixtures/mini.xlsb");
    uint64_t full_cells = 0;
    ZipIndex mz = open_zip_file("tests/fixtures/mini.xlsb");
    for (const auto& nm : mz.names()) {
        if (nm.compare(0, 19, "xl/worksheets/sheet") != 0 || nm.find(".bin") == std::string::npos) continue;
        if (nm.find("binaryIndex") != std::string::npos) continue;
        const auto* b = fz.bytes(nm);
        if (!b) continue;
        for (const auto& r : read_records(*b))
            if (r.id >= 0x01 && r.id <= 0x0B) full_cells++;
    }
    CHECK(xs.cells == full_cells);

    std::vector<uint8_t> chunked;
    auto raw = mz.extract("xl/worksheets/sheet1.bin");
    CHECK(!raw.empty());
}
