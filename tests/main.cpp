#include "framework.hpp"
#include <cstdio>

namespace fvetest {
int checks = 0;
int fails = 0;
const char* cur = "";
}

int main() {
    test_interval();
    test_affine();
    test_types();
    test_ast();
    test_merkle();
    test_holes();
    test_enclosure();
    test_verifier();
    test_proof();
    test_engine();
    test_formula();
    test_zip();
    test_xml();
    test_xlsx();
    test_xlsb();
    test_dim();
    test_bundle();
    test_oracle();
    test_sdg();
    test_ocr();
    test_pipeline();
    test_lower();
    test_store();
    test_registry();
    test_audit();
    test_semantic();
    test_wrapper();
    test_runtime();
    test_service();
    test_http();
    test_grpc();
    test_sqldb();
    test_l0();
    test_criticality();
    test_sivia();
    test_casebase();
    test_diff();
    test_incremental();
    test_artifact_graph();
    test_coverage();
    test_watcher();
    std::printf("\nchecks: %d   fails: %d\n", fvetest::checks, fvetest::fails);
    if (fvetest::fails == 0) std::printf("ALL PASS\n");
    return fvetest::fails == 0 ? 0 : 1;
}
