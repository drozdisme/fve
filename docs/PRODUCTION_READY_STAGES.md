# FVE Production-Readiness Layer

Implements `FVE_PRODUCTION_READY.md`: the trust and interface layer around the
verified core. No change weakens the soundness invariant.

## 1. Compliance
- Identity (`platform/identity`): users with roles (engineer, production_operator,
  reviewer, admin) and section scope; append-only electronic `signatures` with a
  human-readable meaning. `Audit::record` carries an actor. Migration 0010.
- Configuration snapshot: `CritConfig` with a canonical hash; recorded at startup;
  every run is stamped with `config_hash` and `engine_version`. Migration 0011.
- Change control: a fix is `proposed` until a reviewer signs and approves it; only
  approved fixes are auto-transferred by the Case Base. Endpoints `POST /api/users`,
  `GET /api/users`, `POST /api/sign`, `GET /api/signatures`, `POST /api/fixes/:id/approve`,
  `GET /api/config`.

## 2. Production interface
- `POST /api/production/report` maps a scanned part and issue type onto the
  verification loop and returns a sober status (green/yellow/red), a ready message,
  and an ETA when an engineer is dispatched. `GET /api/production/report/:id`
  returns current status. The status is rendered without decorative colour.

## 3. Observability
- Coverage Ledger (`platform/coverage`): per-file total/formula/constant/hole-cell
  counts, classified reasons, self-confidence, and a global unsupported-construct
  heatmap. Endpoints `GET /api/coverage/:id`, `GET /api/coverage/heatmap`,
  `GET /api/coverage/sample`. Migration 0012.

## 4. Excel path A
- Folder watcher (`l0/watcher`, `bin/watch`): polling by (mtime, sha256) against the
  registry; re-verifies changed heads. Hole resolution endpoint
  `POST /api/holes/:id/resolve` records a proposed fix for reviewer approval.

## 5. SQL section mapping
- `section_map` built from the L0 folder tree; production-facing `section_status`
  aggregation. Endpoints `GET /api/sections/status`, `POST /api/sections/map`.
  Migrations 0013, 0014 (`edit_mode` per file for the future structured editor).

## Interface principles
- Strict, professional presentation: restrained palette, no decorative colour, no
  emoji. Status is conveyed by label and a single muted indicator.
