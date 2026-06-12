# Architecture Compliance Report — Stage 3

This report maps each Stage 3 requirement to its implementation and shows the end
-to-end validation pipeline. Stages 1 and 2 are unchanged; Stage 3 adds the
autonomous platform on top of them.

## Required deliverables

| Deliverable | Location | Status |
|---|---|---|
| `semantic/` | `semantic/glue.{hpp,cpp}` | implemented |
| `wrapper/` | `wrapper/synth.{hpp,cpp}` | implemented |
| `registry/` | `registry/registry.{hpp,cpp}` | implemented |
| `db/` | `db/store.{hpp,cpp}`, `db/schema.sql`, `db/migrations/` | implemented |
| `api/` | `api/http.{hpp,cpp}`, `api/proto/fve.proto`, `cmd/server.cpp` | REST implemented, gRPC defined |
| `ui/` | `ui/index.html`, `ui/app.js`, `ui/style.css` | implemented |
| `audit/` | `audit/audit.{hpp,cpp}` | implemented |
| `deploy/` | `deploy/Dockerfile`, `deploy/docker-compose.yml`, `deploy/k8s/`, `.github/workflows/ci.yml`, `deploy/production.env` | implemented |

## Semantic gluing

`semantic/glue.cpp` implements the sheaf-style consistency layer deferred in
Stage 2:

- **section matching** — `match_sections` aligns symbols shared between two
  regions.
- **overlap resolution** — symbol assignments on overlapping regions are
  combined by interval meet (the restriction maps must agree).
- **conflict detection** — a non-overlapping range meet (`range`) or a
  disagreeing dimension (`dimension`) is recorded as a `Conflict`.
- **obstruction detection** — any conflict marks the global section
  `obstructed`: the local sections cannot be glued into a consistent global one.

## Wrapper synthesis

`wrapper/synth.cpp` implements the active loop deferred in Stage 1:

- **active probing** — `probe_model` samples the opaque function on a grid over
  its input box.
- **local model construction** — it estimates a Lipschitz constant from the
  sampled secants and detects monotonicity in one dimension.
- **interval wrapping** — `synth_wrapper` builds a rigorous Stage 1
  `lipschitz_wrapper` from the probes and a safety-inflated constant.
- **uncertainty enclosure** — `wrap_enclose` returns the sound enclosure of the
  opaque output over a query box. This is what lets the platform close a hole and
  turn an `ABSTAIN` into a decision, conditional on the assumed regularity class.

## Oracle runtime

`oracle/runtime.cpp` wraps the deterministic Stage 2 oracle:

- **isolated execution / workbook sandbox** — evaluation runs entirely inside the
  in-process deterministic interpreter; no spreadsheet application or external
  code is executed, so isolation is structural.
- **execution monitoring** — `RunLimits`/`RunStats` cap cells and evaluations and
  report limit hits.
- **artifact capture** — `capture()` records the input and output cell values as a
  JSON artifact for provenance.

## Registry

`registry/registry.cpp` provides the **model** and **artifact** registries with
content-addressed ids (`sha256`), **versioning** (per-key incrementing version,
identical content de-duplicates to the same id), and **provenance** links
(`artifact -[produced]-> model`, and from the service `artifact -[verified]-> run`).

## Database

`db/schema.sql` defines the PostgreSQL schema (artifacts, models, runs, targets,
certificates, provenance, audit) with foreign keys and indexes; `db/migrations/`
holds the ordered, transactional migrations. The **persistence layer**
(`db/store.cpp`) is a document store with a file backend that runs with no
database server; the same interface is satisfied by the SQL schema for the
managed deployment.

## API

`api/http.cpp` is a dependency-free HTTP/1.1 server (routing with path
parameters, query parsing, multipart and raw uploads, static file serving, CORS).
`cmd/server.cpp` exposes upload, analysis, verification, certificate retrieval,
artifact retrieval, graph, provenance, and audit. `api/proto/fve.proto` defines
the equivalent gRPC service; the live transport is REST (see Unresolved issues).

## Frontend

`ui/` is a dependency-free single-page app: an **upload page**, a **verdict
dashboard**, an SVG **graph viewer** of the semantic dependency graph, a
**certificate viewer**, and an audit view. It talks to the REST API only.

## Audit system

`audit/audit.cpp` builds on the Stage 1 Merkle `AuditChain`, persisted to the
store: **provenance tracking** and **artifact lineage** (`lineage`), **execution
history** (`history`, optionally filtered by subject), and **certificate history**
(`certificates`). `verify()` recomputes the chain to prove tamper-evidence.

## Validation pipeline

`platform/service.cpp` orchestrates the required flow and records every stage to
the audit chain:

```
xlsx / xlsm / xlsb / image
  -> Multimodal Lift          extract_file            (audit: lift)
  -> Interventional Graph      induce_deps             (audit: interventional_graph)
  -> Semantic Gluing           glue(sections_of(...))  (audit: semantic_gluing)
  -> Holed Program             lower_target
  -> Wrapper Synthesis         probe_model/wrap_enclose(audit: wrapper_synthesis)
  -> Enclosure Program         Stage 1 engine
  -> Certificate               proof::validate         (audit: certificate)
  -> SAFE / FAIL / ABSTAIN     verdict
```

Verified at runtime over the HTTP API:

- `beam.xlsx` → analyze (`glue_consistent=true`) → verify → two targets `SAFE`,
  certificates validate, audit chain intact.
- `holed.xlsx` → the `IF` lowers to a disjunction and `VLOOKUP` to a hole, so the
  interval path `ABSTAIN`s; wrapper synthesis probes the oracle over the input box
  and encloses the margin as `[0.52, 0.81]` → `SAFE` via `method=wrapper_synthesis`.

## Completion criteria

1. **Upload artifacts** — `POST /api/upload`, UI upload page.
2. **Generate formal models** — `POST /api/artifacts/:id/analyze` (lift +
   interventional graph + semantic gluing, model registered).
3. **Verify margin of safety** — `POST /api/artifacts/:id/verify`.
4. **Receive machine-verifiable certificates** — `GET /api/runs/:id`; each target
   carries a Stage 1 certificate that `validate()` re-checks.
5. **Review provenance and audit** — `GET /api/artifacts/:id/provenance`,
   `GET /api/audit`, `GET /api/audit/verify`, UI audit view.

## Soundness

Stage 3 adds no path that can manufacture a false `SAFE`. The interval path
inherits the Stage 1 invariant unchanged. Wrapper synthesis is the only path that
can tighten a hole, and it does so through a rigorous Lipschitz enclosure of
oracle samples; its verdicts are explicitly labelled `wrapper_synthesis` and are
sound under the stated regularity assumption. Semantic obstruction and unresolved
ambiguity only widen enclosures toward `ABSTAIN`.
