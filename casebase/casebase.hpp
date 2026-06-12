#pragma once
#include "../core/enclosure/enclosure.hpp"
#include "../core/proof/proof.hpp"
#include "../core/verifier/verifier.hpp"
#include "../db/store.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fve {

struct Case {
    std::string id;
    std::string program_hash;
    std::string artifact_id;
    Box box;
    std::map<std::string, double> norm; // midpoints
    Verdict verdict = Verdict::Abstain;
    double cs = 0.0;
    std::string cert_hash;
};

struct Fix {
    std::string id;
    std::string description;
    std::map<std::string, Ival> box_patch;
};

struct CaseMatch {
    Case ref;
    double distance = 0.0;
    Fix fix;
    bool transfer_verified = false;
    Certificate transfer_cert;
};

double case_distance(const Box& a, const Box& b);

class CaseBase {
public:
    explicit CaseBase(Store& s) : store_(s) {}

    std::string record(const Case& c);
    void record_fix(const Fix& f);
    void approve_fix(const std::string& fix_id, const std::string& approver);
    void record_outcome(const std::string& case_id, const std::string& fix_id,
                        const std::string& outcome, const std::string& verified_cert);

    std::vector<CaseMatch> query(const std::string& program_hash, const Box& box,
                                 double delta = 0.15, int top_k = 5) const;

    // Apply fix to a copy of p, re-verify (eval + validate). nullopt unless SAFE.
    std::optional<CaseMatch> try_transfer(const Program& p, const CaseMatch& m) const;

    static std::string make_id(const std::string& program_hash, const Box& box);

private:
    Store& store_;
};

}
