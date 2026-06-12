#include "store.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

namespace fve {

namespace {
std::string sanitize(const std::string& s) {
    std::string o;
    for (char c : s) o += (c == '/' || c == '\\' || c == '.' || c == ' ') ? '_' : c;
    return o;
}
}

Store::Store(const std::string& root) : root_(root) { ensure(root_); }

void Store::ensure(const std::string& dir) const { ::mkdir(dir.c_str(), 0755); }

std::string Store::path(const std::string& coll, const std::string& id) const {
    return root_ + "/" + sanitize(coll) + "/" + sanitize(id) + ".json";
}

bool Store::put(const std::string& coll, const std::string& id, const JsonP& doc) {
    std::lock_guard<std::mutex> lk(mu_);
    ensure(root_ + "/" + sanitize(coll));
    std::ofstream f(path(coll, id), std::ios::trunc);
    if (!f) return false;
    f << dump(doc);
    return true;
}

JsonP Store::get(const std::string& coll, const std::string& id) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::ifstream f(path(coll, id));
    if (!f) return nullptr;
    std::stringstream ss;
    ss << f.rdbuf();
    return parse(ss.str());
}

bool Store::exists(const std::string& coll, const std::string& id) const {
    struct stat st;
    return ::stat(path(coll, id).c_str(), &st) == 0;
}

std::vector<std::string> Store::list(const std::string& coll) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<std::string> out;
    std::string dir = root_ + "/" + sanitize(coll);
    DIR* d = ::opendir(dir.c_str());
    if (!d) return out;
    dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n.size() > 5 && n.substr(n.size() - 5) == ".json")
            out.push_back(n.substr(0, n.size() - 5));
    }
    ::closedir(d);
    return out;
}

bool Store::append(const std::string& log, const JsonP& entry) {
    std::lock_guard<std::mutex> lk(mu_);
    ensure(root_ + "/_logs");
    std::ofstream f(root_ + "/_logs/" + sanitize(log) + ".ndjson", std::ios::app);
    if (!f) return false;
    f << dump(entry) << "\n";
    return true;
}

std::vector<JsonP> Store::read_log(const std::string& log) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<JsonP> out;
    std::ifstream f(root_ + "/_logs/" + sanitize(log) + ".ndjson");
    std::string line;
    while (std::getline(f, line))
        if (!line.empty()) out.push_back(parse(line));
    return out;
}

}
