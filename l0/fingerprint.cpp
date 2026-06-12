#include <sys/stat.h>
#include "fingerprint.hpp"
#include "../extractor/zip/zip.hpp"
#include "../core/merkle/sha256.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace fve {

const std::vector<DomainSchema>& schema_registry() {
    static const std::vector<DomainSchema> reg = {
        {"skin_stringer_fem", "structural.skin_stringer",
         {"ms_summary", "stringerfemloads", "skin_flows", "aux_skin_input", "aux_stringer_input",
          "stringer", "skin", "buckling", "crippling"}},
        {"stringer_fem_loads", "structural.stringer", {"bay id", "fem load case", "nx", "nxy", "msstr_buck"}},
        {"rivet_joint", "fastener.rivet", {"rivet", "bacr15fv", "shear", "pitch", "rivet diameter"}},
        {"bolt_joint", "fastener.bolt", {"bolt", "torque", "preload", "bolt diameter", "shank"}},
        {"frame_section", "structural.frame", {"frame", "station", "moment of inertia", "bending"}},
        {"panel_buckling", "structural.panel", {"panel", "buckling", "sigma_cr", "kc"}},
    };
    return reg;
}

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Extract printable ASCII tokens from a (possibly UTF-16LE) part. Drops <xml> tags.
void tokens_of(const std::vector<uint8_t>& b, bool strip_tags, std::vector<std::string>& toks, std::string& corpus) {
    std::string cur;
    bool in_tag = false;
    auto flush = [&]() {
        if (cur.size() >= 2) {
            toks.push_back(cur);
            corpus += cur;
            corpus += ' ';
        }
        cur.clear();
    };
    for (size_t i = 0; i < b.size(); i++) {
        uint8_t c = b[i];
        if (c == 0) continue;
        if (strip_tags) {
            if (c == '<') { in_tag = true; flush(); continue; }
            if (c == '>') { in_tag = false; continue; }
            if (in_tag) continue;
        }
        if (c >= 32 && c < 127 && c != ' ' && c != ',' && c != ';' && c != '\t' && c != '\n' && c != '\r')
            cur += (char)c;
        else
            flush();
    }
    flush();
}

Fingerprint classify(const std::string& corpus, const std::string& sig) {
    Fingerprint fp;
    fp.structural_sig = sig;
    std::string lc = lower(corpus);
    int best = 0;
    const DomainSchema* bestsc = nullptr;
    for (const auto& sc : schema_registry()) {
        int hits = 0;
        for (const auto& m : sc.markers)
            if (lc.find(m) != std::string::npos) hits++;
        if (hits > best) {
            best = hits;
            fp.format_type = sc.format_id;
            fp.domain = sc.domain;
            bestsc = &sc;
        }
    }
    fp.marker_hits = best;
    if (best == 0) { fp.format_type = "unknown_format"; fp.domain = "unknown"; }
    else if (bestsc)
        for (const auto& m : bestsc->markers)
            if (lc.find(m) != std::string::npos) fp.markers.push_back(m);
    return fp;
}

Fingerprint from_opc(const ZipIndex& z, bool xml) {
    std::string wb_part = xml ? "xl/workbook.xml" : "xl/workbook.bin";
    std::string ss_part = xml ? "xl/sharedStrings.xml" : "xl/sharedStrings.bin";
    std::vector<std::string> toks;
    std::string corpus;
    auto wb = z.extract(wb_part, 8u << 20);
    tokens_of(wb, xml, toks, corpus);
    std::vector<std::string> struct_toks = toks;
    auto ss = z.extract(ss_part, 32u << 20);
    std::vector<std::string> sstoks;
    tokens_of(ss, xml, sstoks, corpus);

    // Inline-string fallback: scan only SMALL worksheet parts (skip giant sheets),
    // bounded by an aggregate byte budget so huge workbooks stay cheap.
    if (corpus.size() < 200000) {
        size_t budget = 8u << 20;
        for (const auto& name : z.names()) {
            if (name.compare(0, 18, "xl/worksheets/shee") != 0) continue;
            uint32_t us = z.usize_of(name);
            if (us == 0 || us > (2u << 20) || us > budget) continue;
            auto sh = z.extract(name, 2u << 20);
            std::vector<std::string> shtoks;
            tokens_of(sh, xml, shtoks, corpus);
            budget = us > budget ? 0 : budget - us;
            if (corpus.size() > 400000 || budget == 0) break;
        }
    }

    std::set<std::string> uniq(struct_toks.begin(), struct_toks.end());
    std::string canon = "n:" + std::to_string(uniq.size()) + "|";
    for (const auto& t : uniq) canon += t + ",";
    Fingerprint fp = classify(corpus, hex(sha256(canon)));
    fp.parsed = true;
    return fp;
}

}

Fingerprint fingerprint_text(const std::string& head, const std::string& ext) {
    (void)ext;
    Fingerprint fp = classify(head, hex(sha256(head)));
    fp.parsed = true;
    return fp;
}

Fingerprint fingerprint(const std::string& path, const std::string& ext) {
    if (ext == "xlsx" || ext == "xlsm" || ext == "xlsb") {
        ZipIndex z = open_zip_file(path);
        if (z.ok) return from_opc(z, ext != "xlsb");
    } else if (ext == "csv" || ext == "txt" || ext == "tsv") {
        std::ifstream f(path, std::ios::binary);
        std::string head(4096, '\0');
        f.read(&head[0], 4096);
        head.resize((size_t)f.gcount());
        return fingerprint_text(head, ext);
    }
    Fingerprint fp;
    fp.structural_sig = hex(sha256(path));
    return fp;
}

}

namespace fve {

QuickStamp quick_stamp(const std::string& path) {
    QuickStamp q;
    struct stat st;
    if (::stat(path.c_str(), &st) == 0) { q.mtime = (long)st.st_mtime; q.size = (long)st.st_size; }
    return q;
}

FullStamp full_stamp(const std::string& path) {
    FullStamp f;
    f.quick = quick_stamp(path);
    f.sha256 = hex(sha256_file(path));
    return f;
}

}
