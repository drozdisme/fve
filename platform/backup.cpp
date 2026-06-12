#include "backup.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sys/stat.h>

namespace fve {

namespace {
bool safe_path(const std::string& p) {
    for (char c : p) if (c == '\'' || c == '`' || c == '$' || c == ';' || c == '\n') return false;
    return true;
}
std::string stamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H%M%S", &tmv);
    return buf;
}
}

BackupResult run_backup(const std::string& data_dir, const std::string& backup_dir,
                        const std::string& merkle_root) {
    BackupResult r;
    if (!safe_path(data_dir) || !safe_path(backup_dir)) { r.error = "unsafe_path"; return r; }
    ::mkdir(backup_dir.c_str(), 0755);
    std::string ts = stamp();
    std::string archive = backup_dir + "/" + ts + ".tgz";
    // -C parent to avoid absolute paths inside the archive
    std::string cmd = "tar czf '" + archive + "' -C '" + data_dir + "' . 2>/dev/null";
    int rc = std::system(cmd.c_str());
    if (rc != 0) { r.error = "tar_failed"; return r; }
    struct stat st;
    if (::stat(archive.c_str(), &st) == 0) r.size_bytes = (long)st.st_size;
    std::ofstream side(backup_dir + "/" + ts + ".merkle");
    side << merkle_root << "\n";
    r.path = archive;
    r.merkle_root = merkle_root;
    r.ok = true;
    return r;
}

}
