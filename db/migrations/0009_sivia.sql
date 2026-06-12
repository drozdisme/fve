BEGIN;
CREATE TABLE IF NOT EXISTS sivia_runs (
    id TEXT PRIMARY KEY, artifact_id TEXT, epsilon DOUBLE PRECISION,
    safe_boxes JSONB, unknown_boxes JSONB, safe_fraction DOUBLE PRECISION,
    iterations INT, created_at TIMESTAMPTZ DEFAULT now());
COMMIT;
