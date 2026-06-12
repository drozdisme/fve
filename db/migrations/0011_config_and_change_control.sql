BEGIN;
CREATE TABLE IF NOT EXISTS config_snapshots (
    hash TEXT PRIMARY KEY, alpha DOUBLE PRECISION, thresholds JSONB,
    schema_registry_version TEXT, case_base_snapshot_id TEXT,
    created_at TIMESTAMPTZ DEFAULT now());
ALTER TABLE runs ADD COLUMN IF NOT EXISTS config_hash TEXT REFERENCES config_snapshots(hash);
ALTER TABLE runs ADD COLUMN IF NOT EXISTS engine_version TEXT;
ALTER TABLE production_events ADD COLUMN IF NOT EXISTS config_hash TEXT REFERENCES config_snapshots(hash);
ALTER TABLE fixes ADD COLUMN IF NOT EXISTS status TEXT NOT NULL DEFAULT 'proposed';
COMMIT;
