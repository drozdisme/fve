#include "crawler.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace fve {

std::string lower_ext(const std::string& name) {
    size_t d = name.rfind('.');
    if (d == std::string::npos) return "";
    std::string e = name.substr(d + 1);
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e;
}

namespace {

bool wanted(const std::string& ext, const std::vector<std::string>& exts) {
    if (exts.empty()) return true;
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

void walk(const std::string& abs, const std::string& rel, int dir_idx,
          const std::vector<std::string>& exts, long since, CrawlResult& out,
          std::set<std::pair<long, long>>& visited, int depth_guard) {
    if (depth_guard > 64) { out.errors.push_back({abs, "max_depth"}); return; }
    DIR* d = ::opendir(abs.c_str());
    if (!d) { out.errors.push_back({abs, "opendir_failed"}); return; }
    std::vector<std::pair<std::string, std::string>> subdirs;
    dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == ".." || (!n.empty() && n[0] == '.')) continue;
        std::string ap = abs + "/" + n;
        std::string rp = rel.empty() ? n : rel + "/" + n;
        struct stat st;
        if (::lstat(ap.c_str(), &st) != 0) { out.errors.push_back({ap, "lstat_failed"}); continue; }
        bool is_link = S_ISLNK(st.st_mode);
        if (is_link && ::stat(ap.c_str(), &st) != 0) { out.errors.push_back({ap, "dangling_symlink"}); continue; }
        if (S_ISDIR(st.st_mode)) {
            std::pair<long, long> key{(long)st.st_dev, (long)st.st_ino};
            if (visited.count(key)) { out.errors.push_back({ap, "cycle_skipped"}); continue; }
            visited.insert(key);
            subdirs.push_back({ap, rp});
        } else if (S_ISREG(st.st_mode)) {
            out.scanned++;
            std::string ext = lower_ext(n);
            if (!wanted(ext, exts)) continue;
            if (since > 0 && (long)st.st_mtime <= since) { out.skipped++; continue; }
            FsFile f;
            f.path = ap;
            f.rel = rp;
            f.name = n;
            f.ext = ext;
            f.dir = dir_idx;
            f.size = (long)st.st_size;
            f.mtime = (long)st.st_mtime;
            out.files.push_back(f);
        }
    }
    ::closedir(d);
    std::sort(subdirs.begin(), subdirs.end());
    for (auto& sd : subdirs) {
        FsDir fd;
        fd.path = sd.first;
        fd.rel = sd.second;
        size_t sl = sd.second.rfind('/');
        fd.name = sl == std::string::npos ? sd.second : sd.second.substr(sl + 1);
        fd.parent = dir_idx;
        fd.depth = out.dirs[dir_idx].depth + 1;
        int idx = (int)out.dirs.size();
        out.dirs.push_back(fd);
        walk(sd.first, sd.second, idx, exts, since, out, visited, depth_guard + 1);
    }
}

}

CrawlResult crawl(const std::string& root, const std::vector<std::string>& exts, long since_mtime) {
    CrawlResult out;
    FsDir r;
    r.path = root;
    r.rel = "";
    r.name = ".";
    r.parent = -1;
    r.depth = 0;
    out.dirs.push_back(r);
    std::set<std::pair<long, long>> visited;
    struct stat st;
    if (::stat(root.c_str(), &st) == 0) visited.insert({(long)st.st_dev, (long)st.st_ino});
    walk(root, "", 0, exts, since_mtime, out, visited, 0);
    return out;
}

}
