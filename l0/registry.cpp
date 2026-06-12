#include "registry.hpp"
#include "fingerprint.hpp"
#include "version.hpp"
#include "../core/merkle/sha256.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace fve {

namespace {

std::string read_sha(const std::string& path) {
    return hex(sha256_file(path));
}

std::string sqlesc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\'') o += "''";
        else o += c;
    }
    return o;
}

}

Catalog build_registry(const std::string& root, const std::vector<std::string>& exts, long since_mtime) {
    Catalog reg;
    reg.root = root;
    CrawlResult cr = crawl(root, exts, since_mtime);
    reg.errors = cr.errors;
    for (const auto& d : cr.dirs) {
        Folder fo;
        fo.id = (int)reg.folders.size();
        fo.path = d.path;
        fo.rel = d.rel;
        fo.name = d.name;
        fo.parent = d.parent;
        fo.depth = d.depth;
        reg.folders.push_back(fo);
    }
    for (const auto& f : cr.files) {
        FileEntry e;
        e.folder = f.dir;
        e.path = f.path;
        e.rel = f.rel;
        e.name = f.name;
        e.ext = f.ext;
        e.size = f.size;
        e.mtime = f.mtime;
        e.sha256 = read_sha(f.path);
        e.id = "file_" + hex(sha256(f.rel)).substr(0, 16);
        Fingerprint fp = fingerprint(f.path, f.ext);
        e.format_type = fp.format_type;
        e.domain = fp.domain;
        e.structural_sig = fp.structural_sig;
        e.marker_hits = fp.marker_hits;
        std::string kw;
        for (const auto& m : fp.markers) kw += m + " ";
        e.keywords = kw;
        Rev rv = parse_revision(f.name);
        e.config_key = rv.config_key;
        e.revision = rv.label;
        e.rank = rv.rank;
        reg.files.push_back(e);
    }
    resolve_heads(reg);
    return reg;
}

void resolve_heads(Catalog& reg) {
    std::map<std::string, std::vector<int>> groups;
    for (size_t i = 0; i < reg.files.size(); i++)
        groups[reg.files[i].config_key].push_back((int)i);
    for (auto& kv : groups) {
        auto& idx = kv.second;
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            const FileEntry& x = reg.files[a];
            const FileEntry& y = reg.files[b];
            if (x.rank != y.rank) return x.rank > y.rank;
            if (x.mtime != y.mtime) return x.mtime > y.mtime;
            int da = reg.folders[x.folder].depth, db = reg.folders[y.folder].depth;
            if (da != db) return da > db;
            return x.rel > y.rel;
        });
        std::string vg = "vg_" + hex(sha256(kv.first)).substr(0, 12);
        for (size_t k = 0; k < idx.size(); k++) {
            FileEntry& e = reg.files[idx[k]];
            e.is_head = (k == 0);
            e.version_group = vg;
            e.version_index = (int)k;
        }
    }
}

namespace {

JsonP folder_doc(const Folder& fo) {
    auto d = Json::mkobj();
    d->obj["id"] = Json::mknum(fo.id);
    d->obj["path"] = Json::mkstr(fo.path);
    d->obj["rel"] = Json::mkstr(fo.rel);
    d->obj["name"] = Json::mkstr(fo.name);
    d->obj["parent_id"] = Json::mknum(fo.parent);
    d->obj["depth"] = Json::mknum(fo.depth);
    return d;
}

JsonP file_doc(const FileEntry& e) {
    auto d = Json::mkobj();
    d->obj["id"] = Json::mkstr(e.id);
    d->obj["folder_id"] = Json::mknum(e.folder);
    d->obj["path"] = Json::mkstr(e.path);
    d->obj["rel_path"] = Json::mkstr(e.rel);
    d->obj["name"] = Json::mkstr(e.name);
    d->obj["ext"] = Json::mkstr(e.ext);
    d->obj["size"] = Json::mknum((double)e.size);
    d->obj["mtime"] = Json::mknum((double)e.mtime);
    d->obj["sha256"] = Json::mkstr(e.sha256);
    d->obj["format_type"] = Json::mkstr(e.format_type);
    d->obj["domain"] = Json::mkstr(e.domain);
    d->obj["structural_sig"] = Json::mkstr(e.structural_sig);
    d->obj["marker_hits"] = Json::mknum(e.marker_hits);
    d->obj["config_key"] = Json::mkstr(e.config_key);
    d->obj["revision"] = Json::mkstr(e.revision);
    d->obj["rev_rank"] = Json::mknum(e.rank);
    d->obj["is_head"] = Json::mkbool(e.is_head);
    d->obj["version_group"] = Json::mkstr(e.version_group);
    d->obj["version_index"] = Json::mknum(e.version_index);
    d->obj["keywords"] = Json::mkstr(e.keywords);
    return d;
}

}

void migrate(const Catalog& reg, Store& store) {
    for (const auto& fo : reg.folders)
        store.put("folders", "fold_" + std::to_string(fo.id), folder_doc(fo));
    for (const auto& e : reg.files)
        store.put("file_registry", e.id, file_doc(e));
    auto idx = Json::mkobj();
    idx->obj["root"] = Json::mkstr(reg.root);
    idx->obj["folders"] = Json::mknum((double)reg.folders.size());
    idx->obj["files"] = Json::mknum((double)reg.files.size());
    int heads = 0, unknown = 0;
    for (const auto& e : reg.files) {
        if (e.is_head) heads++;
        if (e.format_type == "unknown_format") {
            unknown++;
            auto u = Json::mkobj();
            u->obj["id"] = Json::mkstr(e.id);
            u->obj["rel_path"] = Json::mkstr(e.rel);
            u->obj["ext"] = Json::mkstr(e.ext);
            store.append("unknown_format", u);
        }
    }
    for (const auto& er : reg.errors) {
        auto ed = Json::mkobj();
        ed->obj["path"] = Json::mkstr(er.path);
        ed->obj["reason"] = Json::mkstr(er.reason);
        store.append("l0_errors", ed);
    }
    idx->obj["heads"] = Json::mknum(heads);
    idx->obj["unknown_format"] = Json::mknum(unknown);
    idx->obj["errors"] = Json::mknum((double)reg.errors.size());
    store.put("l0", "registry_index", idx);
    store.append("l0_crawl", idx);
}

std::string registry_sql(const Catalog& reg) {
    std::ostringstream o;
    o << "BEGIN;\n";
    for (const auto& fo : reg.folders) {
        o << "INSERT INTO folders(id,path,rel,name,parent_id,depth) VALUES("
          << fo.id << ",'" << sqlesc(fo.path) << "','" << sqlesc(fo.rel) << "','"
          << sqlesc(fo.name) << "',"
          << (fo.parent < 0 ? "NULL" : std::to_string(fo.parent)) << "," << fo.depth
          << ") ON CONFLICT (id) DO NOTHING;\n";
    }
    for (const auto& e : reg.files) {
        o << "INSERT INTO file_registry(id,folder_id,path,rel_path,name,ext,size,mtime,sha256,"
          << "format_type,domain,structural_sig,marker_hits,config_key,revision,rev_rank,"
          << "is_head,version_group,version_index) VALUES('"
          << sqlesc(e.id) << "'," << e.folder << ",'" << sqlesc(e.path) << "','" << sqlesc(e.rel)
          << "','" << sqlesc(e.name) << "','" << sqlesc(e.ext) << "'," << e.size << "," << e.mtime
          << ",'" << sqlesc(e.sha256) << "','" << sqlesc(e.format_type) << "','" << sqlesc(e.domain)
          << "','" << sqlesc(e.structural_sig) << "'," << e.marker_hits << ",'" << sqlesc(e.config_key)
          << "','" << sqlesc(e.revision) << "'," << e.rank << "," << (e.is_head ? "true" : "false")
          << ",'" << sqlesc(e.version_group) << "'," << e.version_index
          << ") ON CONFLICT (id) DO UPDATE SET is_head=EXCLUDED.is_head, mtime=EXCLUDED.mtime;\n";
    }
    o << "COMMIT;\n";
    return o.str();
}

std::vector<int> child_folders(const Catalog& reg, int folder_id) {
    std::vector<int> out;
    for (const auto& fo : reg.folders)
        if (fo.parent == folder_id) out.push_back(fo.id);
    return out;
}

std::vector<const FileEntry*> files_in(const Catalog& reg, int folder_id) {
    std::vector<const FileEntry*> out;
    for (const auto& e : reg.files)
        if (e.folder == folder_id) out.push_back(&e);
    return out;
}

const FileEntry* head_of(const Catalog& reg, const std::string& config_key) {
    for (const auto& e : reg.files)
        if (e.config_key == config_key && e.is_head) return &e;
    return nullptr;
}

}
