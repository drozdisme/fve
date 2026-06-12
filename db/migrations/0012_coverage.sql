BEGIN;
CREATE TABLE IF NOT EXISTS coverage_ledger (
    file_id TEXT REFERENCES file_registry(id), total_cells INT, formula_cells INT,
    constant_cells INT, hole_cells INT, hole_reasons JSONB, confidence TEXT,
    created_at TIMESTAMPTZ DEFAULT now());
CREATE TABLE IF NOT EXISTS unsupported_constructs (
    construct TEXT PRIMARY KEY, file_count INT, cell_count INT, last_seen TIMESTAMPTZ);
COMMIT;
