#pragma once
#include "../core/enclosure/enclosure.hpp"
#include "../core/proof/proof.hpp"
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fve {

std::string box_fingerprint(const Box& box);

class EvalCache {
public:
    struct Entry {
        EvalResult result;
        Certificate cert;
        long computed_at = 0;
    };

    std::optional<Entry> get(const std::string& prog_hash, const Box& box) const;
    void put(const std::string& prog_hash, const Box& box, const EvalResult& r, const Certificate& c);
    void invalidate(const std::string& prog_hash);
    void invalidate_by_symbol(const std::string& sym);
    size_t size() const;
    void set_limit(size_t n);

private:
    std::string key_of(const std::string& ph, const Box& box) const;
    struct Slot {
        Entry e;
        std::string prog_hash;
        std::set<std::string> syms;
    };
    mutable std::map<std::string, Slot> cache_;
    mutable std::list<std::string> lru_;
    size_t limit_ = 10000;
    mutable std::mutex mu_;
};

struct ProgramDiff {
    std::vector<std::string> changed_nodes;   // hashes present in new, absent in old
    std::vector<std::string> unchanged_nodes; // present in both (cacheable)
    std::vector<std::string> new_holes;
    std::vector<std::string> closed_holes;
    bool structure_changed = false;
};

ProgramDiff diff_programs(const Nodep& old_root, const Nodep& new_root);

}
