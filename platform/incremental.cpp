#include "incremental.hpp"
#include "../core/merkle/sha256.hpp"
#include <algorithm>
#include <cstdio>
#include <ctime>

namespace fve {

std::string box_fingerprint(const Box& box) {
    std::string s;
    for (const auto& kv : box) {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "%s:%.12g,%.12g;", kv.first.c_str(), kv.second.lo, kv.second.hi);
        s += buf;
    }
    return hex(sha256(s));
}

std::string EvalCache::key_of(const std::string& ph, const Box& box) const {
    return ph + ":" + box_fingerprint(box);
}

std::optional<EvalCache::Entry> EvalCache::get(const std::string& ph, const Box& box) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::string k = key_of(ph, box);
    auto it = cache_.find(k);
    if (it == cache_.end()) return std::nullopt;
    lru_.remove(k);
    lru_.push_front(k);
    return it->second.e;
}

void EvalCache::put(const std::string& ph, const Box& box, const EvalResult& r, const Certificate& c) {
    std::lock_guard<std::mutex> lk(mu_);
    std::string k = key_of(ph, box);
    Slot s;
    s.e = Entry{r, c, (long)std::time(nullptr)};
    s.prog_hash = ph;
    for (const auto& kv : box) s.syms.insert(kv.first);
    if (cache_.find(k) == cache_.end()) lru_.push_front(k);
    else { lru_.remove(k); lru_.push_front(k); }
    cache_[k] = std::move(s);
    while (cache_.size() > limit_ && !lru_.empty()) {
        std::string victim = lru_.back();
        lru_.pop_back();
        cache_.erase(victim);
    }
}

void EvalCache::invalidate(const std::string& ph) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.prog_hash == ph) { lru_.remove(it->first); it = cache_.erase(it); }
        else ++it;
    }
}

void EvalCache::invalidate_by_symbol(const std::string& sym) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.syms.count(sym)) { lru_.remove(it->first); it = cache_.erase(it); }
        else ++it;
    }
}

size_t EvalCache::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return cache_.size();
}

void EvalCache::set_limit(size_t n) {
    std::lock_guard<std::mutex> lk(mu_);
    limit_ = n;
    while (cache_.size() > limit_ && !lru_.empty()) {
        std::string victim = lru_.back();
        lru_.pop_back();
        cache_.erase(victim);
    }
}

namespace {
void collect(const Nodep& n, std::set<std::string>& hashes, std::set<std::string>& holes) {
    if (!n) return;
    hashes.insert(hex(n->hash));
    if (n->kind == Kind::Hole) holes.insert(n->hole);
    for (const auto& a : n->args) collect(a, hashes, holes);
    for (const auto& c : n->cands) collect(c, hashes, holes);
}
}

ProgramDiff diff_programs(const Nodep& old_root, const Nodep& new_root) {
    ProgramDiff d;
    std::set<std::string> ho, hn, holes_o, holes_n;
    collect(old_root, ho, holes_o);
    collect(new_root, hn, holes_n);
    for (const auto& h : hn) {
        if (ho.count(h)) d.unchanged_nodes.push_back(h);
        else d.changed_nodes.push_back(h);
    }
    for (const auto& h : holes_n) if (!holes_o.count(h)) d.new_holes.push_back(h);
    for (const auto& h : holes_o) if (!holes_n.count(h)) d.closed_holes.push_back(h);
    std::string ro = old_root ? hex(old_root->hash) : "";
    std::string rn = new_root ? hex(new_root->hash) : "";
    d.structure_changed = (ro != rn);
    return d;
}

}
