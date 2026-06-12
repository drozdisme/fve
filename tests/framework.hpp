#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace fvetest {

extern int checks;
extern int fails;
extern const char* cur;

inline void report(bool ok, const char* expr, const char* file, int line) {
    checks++;
    if (!ok) {
        fails++;
        std::printf("  FAIL [%s] %s:%d: %s\n", cur, file, line, expr);
    }
}

inline void report_near(double a, double b, double tol, const char* expr,
                        const char* file, int line) {
    checks++;
    if (!(std::fabs(a - b) <= tol)) {
        fails++;
        std::printf("  FAIL [%s] %s:%d: %s  (%.17g vs %.17g)\n", cur, file, line,
                    expr, a, b);
    }
}

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9e3779b97f4a7c15ULL) {}
    uint64_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    double uniform(double lo, double hi) {
        double u = (next() >> 11) * (1.0 / 9007199254740992.0);
        return lo + u * (hi - lo);
    }
    int range(int n) { return (int)(next() % (uint64_t)n); }
};

}

#define CHECK(c) ::fvetest::report((c), #c, __FILE__, __LINE__)
#define NEAR(a, b, t) ::fvetest::report_near((a), (b), (t), #a " ~ " #b, __FILE__, __LINE__)

void test_interval();
void test_affine();
void test_types();
void test_ast();
void test_merkle();
void test_holes();
void test_enclosure();
void test_verifier();
void test_proof();
void test_engine();
void test_formula();
void test_zip();
void test_xml();
void test_xlsx();
void test_xlsb();
void test_dim();
void test_bundle();
void test_oracle();
void test_sdg();
void test_ocr();
void test_pipeline();
void test_lower();
void test_store();
void test_registry();
void test_audit();
void test_semantic();
void test_wrapper();
void test_runtime();
void test_service();
void test_http();
void test_grpc();
void test_sqldb();
void test_l0();
void test_criticality();
void test_sivia();
void test_casebase();
void test_diff();
void test_incremental();
void test_artifact_graph();
void test_coverage();
void test_watcher();
