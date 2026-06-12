BEGIN;
CREATE TABLE IF NOT EXISTS artifacts (
    id TEXT PRIMARY KEY, name TEXT NOT NULL, mime TEXT NOT NULL,
    hash TEXT NOT NULL, version INT NOT NULL DEFAULT 1, size BIGINT NOT NULL,
    blob BYTEA, created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE TABLE IF NOT EXISTS models (
    id TEXT PRIMARY KEY, artifact_id TEXT NOT NULL REFERENCES artifacts(id),
    hash TEXT NOT NULL, version INT NOT NULL DEFAULT 1, format TEXT,
    sheets INT, inputs INT, dep_edges INT, sdg_nodes INT, sdg_edges INT,
    glue_consistent BOOLEAN, glue_obstructed BOOLEAN, body JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE TABLE IF NOT EXISTS runs (
    id TEXT PRIMARY KEY, artifact_id TEXT NOT NULL REFERENCES artifacts(id),
    rel_dev DOUBLE PRECISION NOT NULL, audit_chain_ok BOOLEAN NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE TABLE IF NOT EXISTS targets (
    id BIGSERIAL PRIMARY KEY, run_id TEXT NOT NULL REFERENCES runs(id),
    cell TEXT NOT NULL, label TEXT, verdict TEXT NOT NULL,
    ms_lo DOUBLE PRECISION, ms_hi DOUBLE PRECISION, method TEXT,
    holes INT, cert_hash TEXT, cert_valid BOOLEAN);
CREATE TABLE IF NOT EXISTS certificates (
    hash TEXT PRIMARY KEY, run_id TEXT REFERENCES runs(id), verdict TEXT NOT NULL,
    ms_lo DOUBLE PRECISION, ms_hi DOUBLE PRECISION, residual DOUBLE PRECISION,
    method TEXT, regime CHAR(1), created_at TIMESTAMPTZ NOT NULL DEFAULT now());
COMMIT;
