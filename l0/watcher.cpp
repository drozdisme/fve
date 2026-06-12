#include "watcher.hpp"
#include "crawler.hpp"
#include "../core/merkle/sha256.hpp"
#include "../platform/service.hpp"
#include <chrono>
#include <thread>

namespace fve {

FolderWatcher::FolderWatcher(const std::string& root, Service& svc, int interval_ms)
    : root_(root), svc_(svc), interval_ms_(interval_ms) {}

std::string FolderWatcher::file_id_for(const std::string& rel) {
    return "file_" + hex(sha256(rel)).substr(0, 16);
}

std::vector<std::string> FolderWatcher::poll_once() {
    std::vector<std::string> changed;
    CrawlResult cr = crawl(root_, {"xlsx", "xlsm", "xlsb", "csv"});
    for (const auto& f : cr.files) {
        std::string sha = hex(sha256_file(f.path));
        auto it = seen_.find(f.rel);
        if (it == seen_.end() || it->second != sha) changed.push_back(f.rel);
        seen_[f.rel] = sha;
    }
    return changed;
}

void FolderWatcher::run() {
    running_ = true;
    while (running_) {
        auto changed = poll_once();
        if (!changed.empty()) {
            svc_.crawl(root_);
            for (const auto& rel : changed) svc_.verify_registered(file_id_for(rel));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }
}

void FolderWatcher::stop() { running_ = false; }

}
