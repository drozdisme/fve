# Architecture Summary — Stage 1 Core

This document maps the implemented core onto the platform's formal contract:
the six-functor extraction pipeline, the soundness theorems, and the nine
non-negotiable invariants.

## 1. What this stage delivers

The mathematical and verification backbone that turns a typed expression graph
(possibly carrying opaque or ambiguous nodes) into a rigorously bounded Margin
of Safety and a three-valued verdict accompanied by a machine-checkable
certificate and a Merkle audit leaf.

```
JSON AST  ->  Enclosure Program  ->  Certificate  ->  SAFE / FAIL / ABSTAIN
```

The stages upstream of the enclosure program (recognition, oracle-driven edge
discovery, gluing, active wrapper synthesis) are intentionally out of scope; the
core is built so they plug in without weakening any guarantee.

## 2. The soundness substrate: interval arithmetic

`core/interval` implements interval extensions of the elementary operations
(`+ - * / pow sqrt log exp sin cos tan`) satisfying the inclusion property
`f(X) subset [f](X)` and isotonicity `X subset X' => [f](X) subset [f](X')`.

The four basic operations use hardware **directed rounding** (`fesetround`
toward minus/plus infinity for the lower/upper endpoints), compiled with
`-frounding-math -ffp-contract=off` so the result endpoints are rounded
outward and the enclosure is never optimistic. Transcendental functions are
computed in round-to-nearest and then rounded outward by a fixed ULP slack;
this is rigorous given a bounded library error (the libm-accuracy assumption,
analogous to assumption F1 in the proof document; see "Honest Limitations").
Trigonometric extensions perform exact range reduction over critical points and
return `[-1, 1]` for wide arguments; `tan` returns the entire line across a
pole. Division by an interval containing zero returns the entire line, which is
conservative.

These properties (inclusion + isotonicity) are exactly the hypotheses of the
Fundamental Theorem of Interval Analysis (proof document, Lemma 0.2), under
which the natural interval extension of any straight-line program is itself an
inclusion function. They are exercised by randomized property tests
(`tests/test_interval.cpp`).

## 3. Affine arithmetic: defeating dependency blow-up

`core/affine` represents a quantity as `c + sum_k t_k * eps_k + err`, with
shared noise symbols `eps_k in [-1, 1]` tracking correlations and a rigorous
non-affine tail `err`. The canonical win is that `x - x` collapses to a point
(the shared symbol cancels), whereas naive interval subtraction would double the
width. Multiplication pushes the non-affine part into the rigorous tail. The
conversion to an interval rounds the radius outward, preserving inclusion.

## 4. Dimensional types: a data-free prior

`core/types` models units as the finitely generated abelian group `Z^7` (the SI
base-dimension exponents). Multiplication adds exponent vectors, division
subtracts, powers scale; addition requires equality (compatibility checking).
Physical bounds (`Phys`) attach an a-priori interval to a dimension — the F1
prior that becomes the universal fallback enclosure for unknown quantities.
Symbols carry an id, a dimension, a physical range, and a unit label.

## 5. The expression graph

`core/ast` defines immutable, hash-consed nodes: constants, variables,
operators, named calls, **holes** (opaque nodes with a typed signature and an
F1 range), and **disjunctions** (candidate bundles). Each node stores a
deterministic SHA-256 hash over its canonical byte serialization (kind, operator,
value bits, symbol, dimension, physical range, child hashes), so structurally
identical graphs hash identically and any change is detectable. Nodes serialize
to and from JSON losslessly (hash-preserving round-trip), and `linearize`
produces a straight-line program with common-subexpression sharing — the
straight-line typed term the abstraction functor would emit.

## 6. Holes: supporting unknown components

`core/holes` provides the enclosure semantics for opaque nodes. A hole with no
wrapper falls back to its physical range with a residual equal to half its
width, which widens the program enclosure and drives `ABSTAIN` — the formal
content of "monotone ignorance": absence of knowledge can only lose precision,
never soundness. Two rigorous wrappers are implemented:

- a **monotone wrapper** that, given a known sign of dependence, encloses by
  evaluating at the interval endpoints (exact for monotone sub-functions);
- a **Lipschitz wrapper** that, given probe samples and an inflated Lipschitz
  constant, returns the intersection of the probe cones (Theorem 3B's `Lambda`)
  and **self-refutes**: if any observed secant slope exceeds the assumed
  constant, the class is falsified and the node collapses to its physical range
  (forcing `ABSTAIN`). This is the Popperian asymmetry — conditional rigor or
  honest abstention, never a false `SAFE`.

Disjunctive bundles are enclosed by the hull over candidate enclosures, so
adding candidates (more ambiguity) can only widen the result (Theorem 2,
inclusion monotonicity under semantic ambiguity).

The active probe-selection loop and the Taylor-model trace (Regime A) require
the oracle and are listed as remaining; the enclosure mathematics given probes
or a trace is fully present and tested.

## 7. The enclosure program and the glass box

`core/enclosure` evaluates a graph under a box of input intervals, producing the
`MS` enclosure, an accumulated residual, and a **per-node trace**: every node's
operator, enclosure, width, and whether it is holed and in which regime. This
trace is the inspectable derivation (invariant I9) — every verdict is backed by
a node-by-node account from inputs to `MS`. An affine evaluator is provided as
an alternative path for correlated boxes. The continuous width field replaces
discrete tiers: precision is a quantity, and `ABSTAIN` is simply "too wide to
separate from zero".

## 8. The decision and the certificate

`core/verifier` implements the three-valued rule on the residual-adjusted
enclosure: with `lower = ms_lo - residual` and `upper = ms_hi + residual`,
`lower > 0 => SAFE`, `upper < 0 => FAIL`, otherwise `ABSTAIN`. A tangential
margin (`lower <= 0 <= upper`), an entire enclosure, or a NaN all yield
`ABSTAIN`. The residual strictly absorbs approximation error (Corollary 3.1), so
it can never flip a true negative into a reported `SAFE`.

`core/proof` packages the verdict into a `Certificate` holding the program hash,
the enclosure bounds, the residual, the safety factor, the method, the regime,
the abstain conditions, and a SHA-256 digest over the canonical certificate
JSON. The **validator is the executable form of invariant I2**: it recomputes
the digest and re-derives the verdict, accepting `SAFE` only when
`ms_lo - residual > 0` and `FAIL` only when `ms_hi + residual < 0`. A forged
certificate that claims `SAFE` without a positive rigorous lower bound is
rejected — this is checked directly in `tests/test_proof.cpp`.

## 9. Audit chain

`core/merkle` provides an own SHA-256 (validated against the standard vectors),
domain-separated Merkle trees with membership proofs, and a tamper-evident audit
chain where each entry hashes its sequence number, payload, and predecessor.
Every run appends the certificate leaf to the chain; any post-hoc edit breaks
the linkage (invariant I5).

## 10. Invariant coverage in this stage

| Invariant | Where it lives in the core |
|---|---|
| I2 safe failure | `verifier` rule + `proof::validate` reject `SAFE` without `ms_lo - residual > 0` |
| I3 corruption costs precision | holes/disjunctions only widen the enclosure (hull, physical-range fallback) |
| I5 immutability and audit | `merkle` Merkle tree + audit chain |
| I9 glass box | per-node enclosure trace from `enclosure::eval` |

Invariants tied to the offline plane (I1 oracle truth, I4 one-way flow, I6
placement invariance, I7 per-template work, I8 separation of duties) are
properties of the surrounding system and the oracle, not of this core; the core
is constructed to be compatible with them.

## 11. Honest limitations

- Soundness is relative to the oracle (F0) and to correct physical-range priors
  (F1); these are domain facts audited once, not per program.
- Transcendental enclosures assume the math library's error is within the fixed
  ULP slack; this holds for standard libm but is an assumption, not a theorem of
  this code.
- Regime-B (Lipschitz) rigor is conditional on the declared class; the wrapper
  refutes the class when it can and abstains otherwise.
- An exactly tangential margin (`MS = 0`) is undecidable by a finite enclosure
  and correctly yields `ABSTAIN`.
