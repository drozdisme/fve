#pragma once
#include "sha256.hpp"
#include <cstdint>
#include <vector>

namespace fve {

Digest leaf_hash(const std::vector<uint8_t>& b);
Digest leaf_hash(const std::string& s);
Digest node_hash(const Digest& l, const Digest& r);

struct Step {
    Digest sib;
    bool right;
};

class Merkle {
public:
    explicit Merkle(const std::vector<Digest>& leaves);
    Digest root() const { return root_; }
    size_t size() const { return n_; }
    std::vector<Step> proof(size_t i) const;
    static bool verify(Digest leaf, const std::vector<Step>& path, const Digest& root);

private:
    size_t n_;
    std::vector<std::vector<Digest>> levels_;
    Digest root_;
};

struct AuditEntry {
    uint64_t seq;
    Digest payload;
    Digest prev;
    Digest hash;
};

class AuditChain {
public:
    AuditChain();
    Digest append(const Digest& payload);
    const std::vector<AuditEntry>& entries() const { return e_; }
    Digest head() const { return head_; }
    bool verify() const;
    static Digest entry_hash(uint64_t seq, const Digest& payload, const Digest& prev);

private:
    std::vector<AuditEntry> e_;
    Digest head_;
};

}
