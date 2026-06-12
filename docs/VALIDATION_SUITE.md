# FVE Validation Suite

Tool-qualification evidence for the FVE verifier. The automated suite contains
35,000+ assertions executed by `make test` (`tests/`). This document maps the
soundness invariants to the tests that exercise them.

## Invariant to test mapping

| Invariant | Where enforced | Tests |
|-----------|----------------|-------|
| SAFE only when `ms_lo - residual > 0` | `core/verifier`, `core/enclosure` | `test_verifier`, `test_enclosure`, `test_proof` |
| Certificate validates the enclosure it certifies | `core/proof` | `test_proof` |
| Audit chain is tamper-evident (Merkle) | `audit/` | `test_audit` |
| Criticality is routing-only, never alters verdict | `core/criticality` | `test_criticality` |
| Case-fix transfer re-runs eval()+validate(), refuses non-SAFE | `casebase/` | `test_casebase` |
| Change control: only approved fixes auto-transfer | `casebase/`, `platform/service` | `test_casebase` |
| Eval cache populated only after validate() | `platform/incremental`, `platform/service` | `test_incremental` |
| SIVIA uses the same eval() as the verifier | `core/sivia` | `test_sivia` |
| Cross-artifact hole beyond depth -> physical fallback | `model/lower`, `platform/artifact_graph` | `test_artifact_graph`, `test_lower` |
| Streaming parse equals full parse (byte-exact) | `extractor/`, `core/merkle` | `test_l0` |
| Coverage scan classification | `platform/coverage` | `test_coverage` |
| Folder watcher change detection | `l0/watcher` | `test_watcher` |

## Reproducing

```
make test     # builds bin/run_tests, runs all assertions, prints "ALL PASS"
```

A run prints the total assertion count and a non-zero exit code on any failure.
Engine builds are identified by `runs.engine_version` (currently `fve-1.0.0`) so a
certificate can always be traced to the binary that produced it.

Last reviewed: 2026-06-12.
