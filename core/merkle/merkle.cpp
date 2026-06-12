#include "merkle.hpp"

namespace fve {

Digest leaf_hash(const std::vector<uint8_t>& b) {
    std::vector<uint8_t> v;
    v.reserve(b.size() + 1);
    v.push_back(0x00);
    v.insert(v.end(), b.begin(), b.end());
    return sha256(v);
}

Digest leaf_hash(const std::string& s) {
    return leaf_hash(std::vector<uint8_t>(s.begin(), s.end()));
}

Digest node_hash(const Digest& l, const Digest& r) {
    std::vector<uint8_t> v;
    v.reserve(65);
    v.push_back(0x01);
    v.insert(v.end(), l.begin(), l.end());
    v.insert(v.end(), r.begin(), r.end());
    return sha256(v);
}

Merkle::Merkle(const std::vector<Digest>& leaves) {
    n_ = leaves.size();
    levels_.clear();
    if (n_ == 0) {
        root_ = sha256(std::string(""));
        levels_.push_back({root_});
        return;
    }
    levels_.push_back(leaves);
    while (levels_.back().size() > 1) {
        const auto& cur = levels_.back();
        std::vector<Digest> up;
        for (size_t i = 0; i < cur.size(); i += 2) {
            const Digest& l = cur[i];
            const Digest& r = (i + 1 < cur.size()) ? cur[i + 1] : cur[i];
            up.push_back(node_hash(l, r));
        }
        levels_.push_back(up);
    }
    root_ = levels_.back()[0];
}

std::vector<Step> Merkle::proof(size_t i) const {
    std::vector<Step> path;
    if (i >= n_) return path;
    size_t idx = i;
    for (size_t lv = 0; lv + 1 < levels_.size(); lv++) {
        const auto& cur = levels_[lv];
        bool right = (idx % 2) == 0;
        size_t sib = right ? idx + 1 : idx - 1;
        if (sib >= cur.size()) sib = idx;
        path.push_back({cur[sib], right});
        idx /= 2;
    }
    return path;
}

bool Merkle::verify(Digest leaf, const std::vector<Step>& path, const Digest& root) {
    Digest acc = leaf;
    for (const auto& s : path)
        acc = s.right ? node_hash(acc, s.sib) : node_hash(s.sib, acc);
    return acc == root;
}

Digest AuditChain::entry_hash(uint64_t seq, const Digest& payload, const Digest& prev) {
    std::vector<uint8_t> v;
    v.reserve(8 + 64);
    for (int i = 0; i < 8; i++) v.push_back(uint8_t(seq >> (i * 8)));
    v.insert(v.end(), prev.begin(), prev.end());
    v.insert(v.end(), payload.begin(), payload.end());
    return sha256(v);
}

AuditChain::AuditChain() { head_ = sha256(std::string("fve-audit-genesis")); }

Digest AuditChain::append(const Digest& payload) {
    uint64_t seq = e_.size();
    Digest h = entry_hash(seq, payload, head_);
    e_.push_back({seq, payload, head_, h});
    head_ = h;
    return h;
}

bool AuditChain::verify() const {
    Digest prev = sha256(std::string("fve-audit-genesis"));
    for (const auto& en : e_) {
        if (en.prev != prev) return false;
        if (en.hash != entry_hash(en.seq, en.payload, en.prev)) return false;
        prev = en.hash;
    }
    return prev == head_;
}

}
