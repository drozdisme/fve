BEGIN;
CREATE TABLE IF NOT EXISTS artifact_deps (
    from_artifact TEXT, to_artifact TEXT, symbol TEXT, cell_ref TEXT,
    PRIMARY KEY (from_artifact, to_artifact, symbol));
COMMIT;
