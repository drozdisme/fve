# FVE Autonomous Mode

One binary, one argument. Connect a folder and read the log.

```
bin/fve-run --root /mnt/production-data
```

This single process owns the whole lifecycle, every step idempotent and
self-checking, so restarting at any point is safe and resumes where it left off:

1. Boot and open the file-backed store (`--data`, default `./fve-data`).
2. Apply pending migrations (recorded in `_migrations`, idempotent).
3. Bootstrap identity: if no users exist, create one admin and print its API token
   to the log exactly once.
4. Bootstrap configuration: record the active `CritConfig` snapshot if absent.
5. Initial crawl: build the registry (bounded streaming fingerprint), then analyze
   and verify every head file. Each file is wrapped so a corrupt or oversized file
   logs an error and the crawl continues - one bad file never stops the run.
6. Print a crawl summary: counts by format, by verdict, top uncovered constructs,
   and overall coverage percentage.
7. Start the HTTP API in a background thread.
8. Start the auto-backup loop in a background thread (`FVE_BACKUP_INTERVAL`).
9. Enter the watch loop forever: poll by (mtime, sha256), re-verify changed heads,
   log the verdict and criticality change, and raise an alert on high criticality.

## Environment

```
FVE_LOG_FORMAT      human (default) | json     newline-delimited JSON for machine intake
FVE_LOG_LEVEL       info (default) | debug | warn | error
FVE_WATCH_INTERVAL  60s (default; accepts Ns / Nm / Nh)
FVE_CRAWL_WORKERS   number of crawl workers (default: hardware concurrency)
FVE_BACKUP_DIR      ./fve-backups
FVE_BACKUP_INTERVAL 24h
FVE_PORT            8080
```

## Log

The log is the only interface. Human mode is aligned single-line records with no
decorative colour and no emoji; JSON mode emits one self-contained JSON object per
line. `progress` is self-throttling (at most once per 30s) so a worker pool cannot
flood the log.

## Notes

- The store is file-backed; SQL migrations are tracked and recorded but the data
  lives in the store directory. Backups snapshot that directory and write the audit
  Merkle root to a sidecar for fast integrity checks.
- Crawl workers are bounded by the single-writer Service; the worker pool provides
  resilient, progress-reporting iteration. Concurrent verification is a future
  optimization noted in KNOWN_LIMITATIONS.
