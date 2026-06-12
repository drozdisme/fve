#pragma once
#include <string>

namespace fve {

struct BackupResult {
    std::string path;
    long size_bytes = 0;
    std::string merkle_root;
    bool ok = false;
    std::string error;
};

// Snapshot the file-backed store directory into backup_dir as a timestamped
// archive, writing the audit Merkle root to a sidecar for fast integrity checks.
BackupResult run_backup(const std::string& data_dir, const std::string& backup_dir,
                        const std::string& merkle_root);

}
