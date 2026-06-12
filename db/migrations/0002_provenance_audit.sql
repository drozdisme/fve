BEGIN;
CREATE TABLE IF NOT EXISTS provenance (
    id BIGSERIAL PRIMARY KEY, from_id TEXT NOT NULL, to_id TEXT NOT NULL,
    rel TEXT NOT NULL, created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE TABLE IF NOT EXISTS audit (
    seq BIGINT PRIMARY KEY, type TEXT NOT NULL, subject TEXT,
    prev_hash TEXT NOT NULL, entry_hash TEXT NOT NULL, payload JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE INDEX IF NOT EXISTS idx_models_artifact ON models(artifact_id);
CREATE INDEX IF NOT EXISTS idx_runs_artifact ON runs(artifact_id);
CREATE INDEX IF NOT EXISTS idx_targets_run ON targets(run_id);
CREATE INDEX IF NOT EXISTS idx_prov_from ON provenance(from_id);
CREATE INDEX IF NOT EXISTS idx_prov_to ON provenance(to_id);
CREATE INDEX IF NOT EXISTS idx_audit_subject ON audit(subject);
COMMIT;
