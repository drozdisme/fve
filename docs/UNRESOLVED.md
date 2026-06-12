# Unresolved Issues

This tracks what is deferred or environment-bound. None of it affects verdict
soundness: every limitation degrades precision (more `ABSTAIN`) rather than
weakening a `SAFE`.

## Closed in this iteration

- **Large-workbook reading** — streaming inflate (32 KB window) + streaming
  BIFF12 assembler read every cell of 200 MB+ worksheet parts with bounded
  memory; validated on a real 184 MB file (50.8 M cells under a 250 MB cap).

- **L0 File Intelligence Layer** — filesystem crawler, file registry, format
  fingerprinter, structural signatures, version resolver (`is_head`), and
  migration into the store + PostgreSQL schema, with the folder hierarchy
  preserved as a first-class tree. See `docs/L0_FILE_INTELLIGENCE.md`.
- **Native gRPC surface** — protobuf wire codec and gRPC-Web framing implemented
  from scratch; all eight RPCs served over the same transport and verified live.
- **Semantic cohomology** — real Čech H¹ over the cover's nerve for relational
  (offset/datum) constraints, distinguishing a nontrivial monodromy class from a
  coboundary.
- **Wrapper synthesis scope** — adaptive probing (refine where the secant slope
  is steepest) and arbitrary input dimension.
- **Concurrency** — thread-pooled HTTP server with a thread-safe store and
  service, verified under concurrent load.
- **Visual recognizer** — a non-ML glyph recognizer (bitmap font, projection
  segmentation, template matching) plus a from-scratch PNG decoder; the
  image→lattice→bundle→verify path is wired end to end.
- **Excel coverage** — the common engineering function set (MOD, INT, FLOOR,
  CEILING, ROUND family, PRODUCT, SUMSQ, SUMPRODUCT, IFERROR, inverse/hyperbolic
  trig, DEGREES/RADIANS, LOG10) in both the oracle and the lowering.

## Environment-bound (cannot be exercised offline)

- **Live PostgreSQL connection.** The PG v3 wire-protocol client (`db/pgwire`,
  `db/sqlstore::PgConn`) is implemented and unit-tested at the byte level and
  against an injectable transport with canned server frames. A live TCP handshake
  to a running Postgres is not exercised because no server is available in the
  build sandbox; the file-backed store remains the live backend, and the SQL
  schema + migrations + generated `INSERT`s are ready for `psql`.
- **Docker / Kubernetes runtime.** Manifests and the multi-stage image are
  provided but not built/run here (no container runtime in the sandbox).

## Genuinely deferred

- **Distributed multi-node execution.** The service is concurrent within one
  process; cross-node clustering (shared work queue, sharded oracle pool) is a
  deployment concern. Kubernetes replicas scale stateless instances.
- **Pixel model for arbitrary fonts/handwriting.** The recognizer matches a
  registered bitmap glyph set; a learned model for unconstrained rasters is out
  of scope (and would be an ML component).
- **Long-tail Excel functions and xlsb RPN tokens.** Unmapped functions evaluate
  to `NaN` in the oracle and lower to holes (sound → `ABSTAIN`); uncommon xlsb
  formula tokens fall back to the cached value.
- **Full sheaf gluing beyond H¹.** Conflicts, obstruction, and the H¹ monodromy
  class are computed; higher cohomology over arbitrary covers is not.

## Soundness note

No deferred or environment-bound item can produce a false `SAFE`. Every missing
capability degrades to a wider enclosure (a hole, a disjunction, an open
dimension, an `unknown_format`, or a NaN-guarded `ABSTAIN`).
