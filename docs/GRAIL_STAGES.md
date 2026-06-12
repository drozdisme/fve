# FVE GRAIL — production operating system stages

Implements `FVE_GRAIL.md` on top of the verified core. Soundness invariant
(§12) preserved everywhere: SAFE only when `ms_lo − residual > 0`; criticality is
routing-only; case-transfer and cache populate only after `eval()`+`validate()`;
SIVIA shares the same `eval()`; new audit events extend the Merkle chain.

## A — Numeric criticality  `core/criticality/`
`CS = σ(−α·SNR)`, `SNR = (ms_lo − residual)/(width + residual + ε)`. Priority
P0–P4, NaN→HALT. `explain_abstain` decomposes ABSTAIN into ranked hole/disjunction
contributors with actionable instructions; `reduction_plan` ranks holes by width.
Wired into `verify` output (`cs`, `priority`, `abstain_report`).

## B — Case Base  `casebase/`
`record / query / try_transfer / record_outcome`. Distance = normalised midpoint
metric over shared symbols. `try_transfer` applies a fix, re-runs `eval()` and
`validate()`, and refuses any non-SAFE result. Records a precedent on every verify;
`GET /api/cases/similar`. Migration `0005`.

## C — Production loop  `platform/event.*`
`POST /api/events` → L0 search → verify with actual params → criticality →
P4 HALT / Case-Base auto-fix / Work Order. `GET /api/events`, `/api/events/:id`,
`/api/work_orders`, `PUT /api/work_orders/:id/resolve`, `/api/dashboard/production`.
Migration `0006`.

## D — Incremental + cross-artifact  `platform/incremental.*`, `platform/artifact_graph.*`
`EvalCache` (program_hash + box fingerprint, LRU, invalidate by hash/symbol) — live,
populated only after `validate()`; `GET /api/cache/stats`. `diff_programs` gives the
hash-consed node delta (changed/unchanged/new+closed holes). `ArtifactGraph` with
topological order, cycle detection and `affected_artifacts` (reverse-reachable
cascade). Migration `0007`. External refs already degrade to phys-fallback holes
(invariant #6); recursive cross-file inlining is the remaining tightening.

## E — Diff workflow + engineer UI  `audit/diff.*`, `ui/`
`diff_runs` computes per-target ΔMS, ΔCS and verdict transitions, hottest-first,
with new/closed-hole bookkeeping and a `safe` regression flag. `GET /api/diff`,
`/api/artifacts/:id/history`. UI adds Production dashboard, Work Orders, version
History+Diff, Admissible region, plus criticality badges, `msBar` and abstain
explanations.

## F — SIVIA admissible region  `core/sivia/`
Moore–Skelboe set inversion (`bisect` widest dim), reports safe/unknown boxes and
safe-volume fraction using the same `eval()`. `POST /api/artifacts/:id/admissible_region`.
Migration `0009`.
