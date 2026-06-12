# FVE — Formal Verification Engine

Autonomous, sound-by-construction platform that lifts engineering artifacts into
formal models and returns a rigorous `SAFE / FAIL / ABSTAIN` verdict on the
Margin of Safety with a machine-checkable certificate.

The central guarantee is **soundness relative to the oracle**: `SAFE` is emitted
only when a rigorous lower bound proves `MS > 0`. Uncertainty, corruption,
ambiguity and opacity only widen enclosures and push toward `ABSTAIN`; a false
`SAFE` is structurally impossible.

## Stages

- **Stage 1 — verification core.** Interval/affine arithmetic with directed
  rounding, dimensional types, a hashed expression graph, the enclosure engine,
  holed programs, proof certificates, a Merkle audit chain, three-valued
  decision. See `docs/ARCHITECTURE_SUMMARY.md`.
- **Stage 2 — extraction layer.** xlsx/xlsm/xlsb parsing, the Excel formula
  parser, image extraction with candidate bundles, dimensional inference (Smith
  Normal Form), the deterministic oracle, interventional dependency induction,
  the Semantic Dependency Graph, lowering to a holed program. See
  `docs/STAGE2_ARCHITECTURE.md`.
- **Stage 3 — autonomous platform.** Semantic gluing, wrapper synthesis, oracle
  runtime, registry, persistence + PostgreSQL schema, REST API + gRPC definition,
  web UI, audit system, and deployment. See `docs/COMPLIANCE_REPORT.md`.

- **L0 — File Intelligence Layer.** Filesystem crawler, file registry, format
  fingerprinter, version resolver, and migration into the store + PostgreSQL,
  preserving the folder hierarchy. See `docs/L0_FILE_INTELLIGENCE.md`.

## Pipeline

```
xlsx / xlsm / xlsb / image
  -> Multimodal Lift
  -> Interventional Graph
  -> Semantic Gluing
  -> Holed Program
  -> Wrapper Synthesis
  -> Enclosure Program
  -> Certificate
  -> SAFE / FAIL / ABSTAIN
```

## Build

```
make all          # libcore.a, bin/verify, bin/extract, bin/server
make test         # full suite (Stage 1 + 2 + 3)
```

CMake is also supported:

```
cmake -B build && cmake --build build && ctest --test-dir build
```

Requirements: a C++17 compiler (tested with g++ 13). No external dependencies.

## Run the platform

```
./bin/server ./data ./ui 8080
```

Open `http://localhost:8080`: upload an artifact, analyze it, verify the margin
of safety, inspect the dependency graph and certificate, review the audit trail.

CLIs:

```
bin/verify  examples/safe_margin.json
bin/extract tests/fixtures/beam.xlsx 0.05
bin/crawl   tests/fixtures/tree ./data --sql registry.sql
bin/find    stringer ./data        # one word -> newest version
bin/fve-run --root /data/tree        # autonomous: crawl, verify, watch, log
```

## REST API

```
POST /api/upload                       upload an artifact (raw body or multipart)
GET  /api/artifacts                    list artifacts
GET  /api/artifacts/:id                artifact metadata
POST /api/artifacts/:id/analyze        lift + interventional graph + gluing
POST /api/artifacts/:id/verify?rel=..  verify margin of safety
GET  /api/artifacts/:id/graph          semantic dependency graph
GET  /api/artifacts/:id/provenance     provenance links
GET  /api/runs/:id                     run + certificates
GET  /api/audit[?subject=]             audit history
GET  /api/audit/verify                 audit chain integrity
GET  /api/health                       health check
```

The equivalent gRPC service is defined in `api/proto/fve.proto`.

## Layout

```
l0/          file intelligence: crawler, fingerprint, version, registry
core/        verification core (Stage 1)
extractor/   zip, xml, formula, xlsx, xlsb, ocr, pipeline (Stage 2)
oracle/      deterministic evaluator, interventional induction, runtime
model/       workbook, bundles, dimensional inference, SDG, lowering
semantic/    sheaf-style gluing (section match, conflict, obstruction)
wrapper/     active probing + interval wrapper synthesis
registry/    model + artifact registry, versioning, provenance
db/          persistence layer + PostgreSQL schema and migrations
audit/        provenance, execution history, lineage, certificate history
platform/    full-pipeline orchestration service
api/         HTTP/REST server + gRPC proto definition
ui/          dependency-free single-page app
cmd/         verify, extract, server entry points
deploy/      Dockerfile, compose, Kubernetes manifests, production config
tests/       unit, property, parser, graph, platform, pipeline tests + fixtures
docs/        architecture, migration, deployment, compliance, unresolved
```

## Status

Stages 1, 2 and 3 are functional and tested (full suite green, ~88% line
coverage). Deferred items (native gRPC codegen, libpq backend wiring, the
pixel-level visual recognizer, distributed execution) are listed in
`docs/UNRESOLVED.md`. None can affect verdict soundness.
