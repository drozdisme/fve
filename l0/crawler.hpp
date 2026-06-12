#pragma once
#include <string>
#include <vector>

namespace fve {

struct FsDir {
    std::string path;
    std::string rel;
    std::string name;
    int parent = -1;
    int depth = 0;
};

struct FsFile {
    std::string path;
    std::string rel;
    std::string name;
    std::string ext;
    int dir = 0;
    long size = 0;
    long mtime = 0;
};

struct CrawlError {
    std::string path;
    std::string reason;
};

struct CrawlResult {
    std::vector<FsDir> dirs;
    std::vector<FsFile> files;
    std::vector<CrawlError> errors;
    int scanned = 0;
    int skipped = 0;
};

CrawlResult crawl(const std::string& root, const std::vector<std::string>& exts = {}, long since_mtime = 0);

std::string lower_ext(const std::string& name);

}
