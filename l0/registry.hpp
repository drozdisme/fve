#pragma once
#include "crawler.hpp"
#include "../db/store.hpp"
#include <string>
#include <vector>

namespace fve {

struct Folder {
    int id = 0;
    std::string path;
    std::string rel;
    std::string name;
    int parent = -1;
    int depth = 0;
};

struct FileEntry {
    std::string id;
    int folder = 0;
    std::string path;
    std::string rel;
    std::string name;
    std::string ext;
    long size = 0;
    long mtime = 0;
    std::string sha256;
    std::string format_type;
    std::string domain;
    std::string structural_sig;
    int marker_hits = 0;
    std::string config_key;
    std::string revision;
    double rank = 0.0;
    bool is_head = false;
    std::string version_group;
    int version_index = 0;
    std::string keywords;
};

struct Catalog {
    std::string root;
    std::vector<Folder> folders;
    std::vector<FileEntry> files;
    std::vector<CrawlError> errors;
};

Catalog build_registry(const std::string& root,
                        const std::vector<std::string>& exts = {"xlsx", "xlsm", "xlsb", "csv"},
                        long since_mtime = 0);

void resolve_heads(Catalog& reg);
void migrate(const Catalog& reg, Store& store);
std::string registry_sql(const Catalog& reg);

std::vector<int> child_folders(const Catalog& reg, int folder_id);
std::vector<const FileEntry*> files_in(const Catalog& reg, int folder_id);
const FileEntry* head_of(const Catalog& reg, const std::string& config_key);

}
