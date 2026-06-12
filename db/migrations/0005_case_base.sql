BEGIN;
CREATE TABLE IF NOT EXISTS cases (
    id TEXT PRIMARY KEY, program_hash TEXT NOT NULL,
    artifact_id TEXT, run_id TEXT,
    box_json JSONB NOT NULL, box_norm JSONB NOT NULL,
    verdict TEXT NOT NULL, cs DOUBLE PRECISION, cert_hash TEXT,
    created_at TIMESTAMPTZ DEFAULT now());
CREATE TABLE IF NOT EXISTS fixes (
    id TEXT PRIMARY KEY, description TEXT NOT NULL, patch_json JSONB NOT NULL,
    created_by TEXT, created_at TIMESTAMPTZ DEFAULT now());
CREATE TABLE IF NOT EXISTS case_fix_links (
    case_id TEXT REFERENCES cases(id), fix_id TEXT REFERENCES fixes(id),
    outcome TEXT NOT NULL, verified_cert TEXT, applied_at TIMESTAMPTZ DEFAULT now(),
    PRIMARY KEY (case_id, fix_id));
CREATE INDEX IF NOT EXISTS idx_cases_phash ON cases(program_hash);
CREATE INDEX IF NOT EXISTS idx_cases_verdict ON cases(verdict);
COMMIT;
