# Known Limitations

Formal record of current limitations for the compliance package. Each item states
the limitation, the safety consequence (if any), and the mitigation.

## Verification scope
- Cross-file formula inlining is not yet recursive. External references degrade to
  physical-fallback holes, which is sound (no false SAFE) but may widen the
  enclosure. Mitigation: the artifact dependency graph and cascade re-verification
  are in place; recursive inlining is a tightening optimization, not a correctness gap.
- Coverage Ledger is a static scan (formula text classification), separate from the
  verifier's runtime holes. It is an observability signal for prioritisation, not a
  verification result.

## Function support
- Reference and dynamic-array families (XLOOKUP, INDEX/MATCH, OFFSET, INDIRECT,
  LAMBDA, FILTER, and similar) are not interval-liftable and are reported as
  uncovered constructs in the heatmap rather than verified.

## Environment
- PostgreSQL wire support exists but is validated only against an injectable
  transport; production deployments should run the documented schema migrations.
- The folder watcher uses polling (not inotify) for network-filesystem reliability;
  detection latency equals the configured interval.

## Identity
- The identity layer stores users, roles, and append-only electronic signatures.
  Corporate SSO/LDAP federation is integrated via `external_id` but the federation
  connector itself is deployment-specific and out of scope for the core.

Last reviewed: 2026-06-12.
