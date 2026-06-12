# Remaining Tasks

Stages 1 and 2 implement the verification core and the extraction layer. This
file tracks what is intentionally deferred.

## Done in Stage 1

Interval and affine arithmetic, dimensional types, the hashed expression graph,
the enclosure engine, holed-program support, proof certificates, the Merkle
audit chain, and the three-valued decision. See `docs/ARCHITECTURE_SUMMARY.md`.

## Done in Stage 2

xlsx / xlsm parsing, xlsb (BIFF12) parsing with cached-formula RPN decode, the
Excel formula parser with dependency extraction, embedded image extraction, the
recognition-lattice candidate-bundle system, dimensional inference with an
integer Smith Normal Form solver, the deterministic oracle, interventional
dependency induction, the Semantic Dependency Graph builder, and lowering to a
Stage 1 holed program. See `docs/STAGE2_ARCHITECTURE.md`.

## Deferred (allowed open tasks for Stage 2)

- **Full sheaf implementation.** SDG building merges syntactic and interventional
  dependencies and tracks ambiguity, but the global sheaf-consistency gluing
  (region restriction maps, gluing axiom, cohomological obstruction localizing
  cross-region clashes) is not implemented. Inconsistencies currently surface as
  dimensional clashes and as holes, not as a localized sheaf obstruction.
- **Active probing.** The intervention engine uses one-at-a-time finite-difference
  perturbation. Adaptive experimental design (group testing for edge discovery,
  Shubert/DIRECT probe selection for wrapper synthesis) is not implemented.
- **Distributed execution / cluster scaling.** Everything runs in a single
  process. There is no work queue, sharding, or multi-node oracle pool.

## Deferred from Stage 1 (unchanged)

- The pixel-to-lattice visual recognizer for OCR (a machine-learning component).
  The bundle generation from a recognition lattice is implemented and tested; the
  lattice itself is assumed to come from an upstream recognizer.
- Active wrapper synthesis loop (probe acquisition through the oracle). The
  rigorous enclosure mathematics given probes or a Lipschitz bound is implemented
  in `core/holes`.
- A delta-complete SMT / branch-and-bound verifier for tighter global bounds.
- Materialized inverse problem (admissible region / nearest feasible config).

## Partial, functional but not exhaustive

- **xlsb RPN coverage.** The record reader and value records are complete; the
  formula token decoder covers literals, references, ranges, binary operators and
  common functions. Less common tokens (arrays, names by index, table
  references, some control tokens) fall back to the cached value, so the cell is
  still usable as data.
- **Excel function library.** The oracle and the lowering map the common
  engineering functions. Unmapped functions evaluate to NaN in the oracle and
  lower to holes, which is sound (drives `ABSTAIN`) but not precise.

## Soundness note

None of the deferred or partial items can produce a false `SAFE`. Every missing
capability degrades to a wider enclosure (a hole, a disjunction, an open
dimension, or a NaN-guarded `ABSTAIN`). Completing them improves precision and
lowers the abstain rate; it does not change the soundness of any verdict.
