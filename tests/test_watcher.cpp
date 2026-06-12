#include "../l0/watcher.hpp"
#include "../platform/service.hpp"
#include "framework.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <unistd.h>

using namespace fve;
using namespace fvetest;

namespace {
std::string wtd(const char* tag) {
    long ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/fve_" + std::string(tag) + "_" + std::to_string(getpid()) + "_" + std::to_string(ns);
}
bool has(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}
void write_csv(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << body;
}
}

void test_watcher() {
    cur = "watcher";
    std::string root = wtd("watchroot");
    std::string data = wtd("watchdata");
    std::string cmd = "mkdir -p " + root + "/sec";
    if (std::system(cmd.c_str()) != 0) { CHECK(false); return; }
    write_csv(root + "/sec/a.csv", "PANEL,BUCKLING\n1,2\n");

    Service svc(data);
    FolderWatcher w(root, svc, 10);

    // first pass: the new file is reported as changed
    auto c1 = w.poll_once();
    CHECK(has(c1, "sec/a.csv"));

    // no change -> empty
    auto c2 = w.poll_once();
    CHECK(c2.empty());

    // modify content -> reported again
    write_csv(root + "/sec/a.csv", "PANEL,BUCKLING\n9,9\n");
    auto c3 = w.poll_once();
    CHECK(has(c3, "sec/a.csv"));

    // new file -> reported, old unchanged not reported
    write_csv(root + "/sec/b.csv", "x\n1\n");
    auto c4 = w.poll_once();
    CHECK(has(c4, "sec/b.csv"));
    CHECK(!has(c4, "sec/a.csv"));

    // id derivation is the registry formula
    CHECK(FolderWatcher::file_id_for("sec/a.csv").rfind("file_", 0) == 0);

    std::system(("rm -rf " + root + " " + data).c_str());
}
