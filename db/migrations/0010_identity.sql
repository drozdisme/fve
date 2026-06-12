BEGIN;
CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY, external_id TEXT UNIQUE, display_name TEXT NOT NULL,
    role TEXT NOT NULL, section_scope TEXT[], active BOOLEAN NOT NULL DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT now());
CREATE TABLE IF NOT EXISTS signatures (
    id TEXT PRIMARY KEY, user_id TEXT NOT NULL REFERENCES users(id),
    subject TEXT NOT NULL, meaning TEXT NOT NULL, payload JSONB,
    signed_at TIMESTAMPTZ DEFAULT now());
ALTER TABLE work_orders       ADD COLUMN IF NOT EXISTS resolved_by TEXT REFERENCES users(id);
ALTER TABLE case_fix_links    ADD COLUMN IF NOT EXISTS approved_by TEXT REFERENCES users(id);
ALTER TABLE fixes             ADD COLUMN IF NOT EXISTS created_by  TEXT REFERENCES users(id);
ALTER TABLE production_events ADD COLUMN IF NOT EXISTS reported_by TEXT REFERENCES users(id);
COMMIT;
