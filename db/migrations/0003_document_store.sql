BEGIN;
CREATE TABLE IF NOT EXISTS documents (
    coll TEXT NOT NULL, id TEXT NOT NULL, body JSONB,
    PRIMARY KEY (coll, id));
CREATE TABLE IF NOT EXISTS logs (
    log TEXT NOT NULL, seq BIGINT NOT NULL, entry JSONB,
    PRIMARY KEY (log, seq));
CREATE INDEX IF NOT EXISTS idx_documents_coll ON documents(coll);
CREATE INDEX IF NOT EXISTS idx_logs_log ON logs(log);
COMMIT;
