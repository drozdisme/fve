BEGIN;
CREATE TABLE IF NOT EXISTS production_events (
    id TEXT PRIMARY KEY, part_id TEXT NOT NULL, section TEXT NOT NULL,
    defect_type TEXT NOT NULL, params_json JSONB, file_id TEXT, run_id TEXT,
    cs DOUBLE PRECISION, priority SMALLINT, action TEXT NOT NULL, fix_id TEXT,
    created_at TIMESTAMPTZ DEFAULT now());
CREATE TABLE IF NOT EXISTS work_orders (
    id TEXT PRIMARY KEY, event_id TEXT, assigned_to TEXT,
    status TEXT NOT NULL DEFAULT 'open', cs DOUBLE PRECISION, priority SMALLINT,
    abstain_why TEXT, fix_id TEXT, resolved_at TIMESTAMPTZ, created_at TIMESTAMPTZ DEFAULT now());
CREATE INDEX IF NOT EXISTS idx_events_priority ON production_events(priority DESC);
CREATE INDEX IF NOT EXISTS idx_wo_status ON work_orders(status, priority DESC);
COMMIT;
