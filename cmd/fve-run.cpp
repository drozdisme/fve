#include "../api/http.hpp"
#include "../api/routes.hpp"
#include "../platform/service.hpp"
#include "../platform/logger.hpp"
#include "../platform/backup.hpp"
#include "../l0/watcher.hpp"
#include "../core/ast/json.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using namespace fve;

namespace {

std::string env_or(const char* k, const std::string& def) {
    const char* v = std::getenv(k);
    return v ? std::string(v) : def;
}

int parse_interval_s(const std::string& s, int def) {
    if (s.empty()) return def;
    int n = std::atoi(s.c_str());
    if (n <= 0) return def;
    if (s.back() == 'm') return n * 60;
    if (s.back() == 'h') return n * 3600;
    return n; // seconds (with or without trailing 's')
}

struct Args {
    std::string root;
    std::string data = "./fve-data";
    std::string ui = "./ui";
    int port = 8080;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        auto next = [&](const std::string& d) { return (i + 1 < argc) ? std::string(argv[++i]) : d; };
        if (s == "--root") a.root = next("");
        else if (s == "--data") a.data = next(a.data);
        else if (s == "--ui") a.ui = next(a.ui);
        else if (s == "--port") a.port = std::atoi(next("8080").c_str());
    }
    a.port = std::atoi(env_or("FVE_PORT", std::to_string(a.port)).c_str());
    return a;
}

std::atomic<long> g_seen{0}, g_new{0}, g_errors{0};

}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);
    Logger log = Logger::from_env();

    if (args.root.empty()) {
        log.stage("boot", "missing --root", {}, LogLevel::Error);
        std::fprintf(stderr, "usage: fve-run --root <data-tree> [--data <store-dir>] [--ui <dir>] [--port N]\n");
        return 2;
    }

    log.stage("boot", "starting fve-run", {{"root", args.root}, {"store", args.data}});

    Service svc(args.data); // constructor bootstraps config snapshot

    JsonP mig = svc.apply_migrations("db/migrations");
    log.stage("migrate", "migrations checked", {{"newly_applied", std::to_string((long)mig->n("count"))}});

    JsonP admin = svc.bootstrap_admin();
    if (admin->has("token")) {
        log.stage("identity", "bootstrap admin created",
                  {{"user_id", admin->str("user_id")}, {"token", admin->str("token")},
                   {"note", "save this token now - it will not be shown again"}});
    } else {
        log.stage("identity", "admin user exists, skipping bootstrap", {});
    }

    JsonP cfg = svc.config_snapshot();
    log.stage("config", "active configuration", {{"hash", cfg->str("hash").substr(0, 12)}, {"engine", cfg->str("engine_version")}});

    int workers = std::atoi(env_or("FVE_CRAWL_WORKERS", std::to_string((int)std::thread::hardware_concurrency())).c_str());
    if (workers < 1) workers = 1;

    bool was_empty = svc.registry_empty();
    log.stage("crawl", "starting registry crawl", {{"workers", std::to_string(workers)}, {"root", args.root}});
    svc.crawl(args.root); // builds file_registry (bounded, streaming fingerprint)

    if (was_empty) {
        std::vector<std::string> ids = svc.head_file_ids();
        log.stage("crawl", "analyzing and verifying head files", {{"files", std::to_string(ids.size())}});
        std::atomic<size_t> idx{0};
        auto worker = [&]() {
            for (;;) {
                size_t i = idx.fetch_add(1);
                if (i >= ids.size()) break;
                g_seen++;
                try {
                    JsonP v = svc.verify_registered(ids[i]);
                    if (v && v->has("artifact_id")) svc.analyze(v->str("artifact_id"));
                    g_new++;
                } catch (const std::exception& e) {
                    g_errors++;
                    log.stage("crawl", "file error", {{"file", ids[i]}, {"err", e.what()}}, LogLevel::Warn);
                } catch (...) {
                    g_errors++;
                    log.stage("crawl", "file error", {{"file", ids[i]}, {"err", "unknown"}}, LogLevel::Warn);
                }
                log.progress("crawl", {{"seen", std::to_string(g_seen.load())},
                                       {"new", std::to_string(g_new.load())},
                                       {"errors", std::to_string(g_errors.load())}});
            }
        };
        std::vector<std::thread> pool;
        for (int w = 0; w < workers; w++) pool.emplace_back(worker);
        for (auto& t : pool) t.join();

        JsonP sum = svc.crawl_summary();
        std::string bf, bv, sk;
        for (const auto& kv : sum->at("by_format")->obj) bf += kv.first + "=" + std::to_string((long)kv.second->num) + " ";
        for (const auto& kv : sum->at("by_verdict")->obj) bv += kv.first + "=" + std::to_string((long)kv.second->num) + " ";
        for (const auto& r : sum->at("top_skip_reasons")->arr) sk += r->str("construct") + "=" + std::to_string((long)r->n("cell_count")) + " ";
        char cov[32]; std::snprintf(cov, sizeof(cov), "%.1f%%", sum->n("coverage_pct"));
        log.stage("crawl", "complete", {{"total", std::to_string((long)sum->n("total_files"))},
                                        {"errors", std::to_string(g_errors.load())}});
        log.stage("crawl", "summary", {{"by_format", bf}, {"by_verdict", bv}, {"coverage", cov}, {"top_skip", sk.empty() ? "none" : sk}});
    } else {
        log.stage("crawl", "registry already populated, skipping analyze/verify pass", {});
    }

    // HTTP API in a background thread
    std::thread api_thread([&]() {
        Http http;
        register_routes(http, svc);
        http.static_dir("/", args.ui);
        log.stage("http", "API listening", {{"port", std::to_string(args.port)}});
        http.listen("0.0.0.0", args.port);
    });
    api_thread.detach();

    // Auto-backup in a background thread
    std::string backup_dir = env_or("FVE_BACKUP_DIR", "./fve-backups");
    int backup_hours = parse_interval_s(env_or("FVE_BACKUP_INTERVAL", "24h"), 24 * 3600) / 3600;
    if (backup_hours < 1) backup_hours = 1;
    std::thread backup_thread([&, backup_dir, backup_hours]() {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::hours(backup_hours));
            BackupResult b = run_backup(args.data, backup_dir, svc.audit_head());
            if (b.ok)
                log.stage("backup", "completed", {{"path", b.path}, {"size_mb", std::to_string(b.size_bytes / 1000000)}, {"merkle", b.merkle_root.substr(0, 12)}});
            else
                log.stage("backup", "failed", {{"error", b.error}}, LogLevel::Warn);
        }
    });
    backup_thread.detach();

    // Watch loop - forever
    int interval = parse_interval_s(env_or("FVE_WATCH_INTERVAL", "60s"), 60);
    FolderWatcher watcher(args.root, svc, interval * 1000);
    {
        std::vector<std::string> baseline = watcher.poll_once(); // seed snapshot; initial crawl already verified these
        log.stage("watch", "baseline established", {{"files", std::to_string(baseline.size())}});
    }
    log.stage("watch", "starting watch loop", {{"interval_s", std::to_string(interval)}});
    for (;;) {
        auto start = std::chrono::steady_clock::now();
        std::vector<std::string> changed = watcher.poll_once();
        long ms = (long)std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        log.stage("watch", "cycle complete", {{"changed", std::to_string(changed.size())}, {"duration_ms", std::to_string(ms)}});
        if (!changed.empty()) {
            svc.crawl(args.root);
            for (const auto& rel : changed) {
                std::string fid = FolderWatcher::file_id_for(rel);
                try {
                    JsonP v = svc.verify_registered(fid);
                    if (v && v->has("artifact_id")) svc.analyze(v->str("artifact_id"));
                    double max_cs = 0.0;
                    std::string verdict = "SAFE";
                    if (v && v->has("targets"))
                        for (const auto& t : v->at("targets")->arr) {
                            if (t->has("cs")) max_cs = std::max(max_cs, t->n("cs"));
                            if (t->str("verdict") == "FAIL") verdict = "FAIL";
                            else if (t->str("verdict") == "ABSTAIN" && verdict != "FAIL") verdict = "ABSTAIN";
                        }
                    char cs[16]; std::snprintf(cs, sizeof(cs), "%.3f", max_cs);
                    log.stage("changed", rel, {{"verdict", verdict}, {"max_cs", cs}});
                    if (max_cs >= 0.75)
                        log.alert("criticality_high", rel, {{"max_cs", cs}, {"verdict", verdict}});
                } catch (const std::exception& e) {
                    log.stage("changed", rel, {{"err", e.what()}}, LogLevel::Warn);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
    return 0;
}
