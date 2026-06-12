#pragma once
#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace fve {

class Service;

// Polling watcher (not inotify: works uniformly on network filesystems).
// Compares (mtime, sha256) of the crawled tree against its last snapshot.
class FolderWatcher {
public:
    FolderWatcher(const std::string& root, Service& svc, int interval_ms = 1000);

    // One detection pass: returns rel-paths whose content changed since last poll;
    // updates the internal snapshot. Does not touch the Service.
    std::vector<std::string> poll_once();

    // Blocking loop: on change, refresh the registry and re-verify changed heads.
    void run();
    void stop();

    static std::string file_id_for(const std::string& rel);

private:
    std::string root_;
    Service& svc_;
    int interval_ms_;
    std::atomic<bool> running_{false};
    std::map<std::string, std::string> seen_; // rel -> sha256
};

}
