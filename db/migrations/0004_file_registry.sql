BEGIN;
CREATE TABLE IF NOT EXISTS folders (
    id        INTEGER PRIMARY KEY,
    path      TEXT NOT NULL,
    rel       TEXT NOT NULL,
    name      TEXT NOT NULL,
    parent_id INTEGER REFERENCES folders(id),
    depth     INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS file_registry (
    id             TEXT PRIMARY KEY,
    folder_id      INTEGER NOT NULL REFERENCES folders(id),
    path           TEXT NOT NULL,
    rel_path       TEXT NOT NULL,
    name           TEXT NOT NULL,
    ext            TEXT,
    size           BIGINT,
    mtime          BIGINT,
    sha256         TEXT,
    format_type    TEXT,
    domain         TEXT,
    structural_sig TEXT,
    marker_hits    INTEGER,
    config_key     TEXT,
    revision       TEXT,
    rev_rank       DOUBLE PRECISION,
    is_head        BOOLEAN,
    version_group  TEXT,
    version_index  INTEGER,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_folders_parent   ON folders(parent_id);
CREATE INDEX IF NOT EXISTS idx_freg_folder      ON file_registry(folder_id);
CREATE INDEX IF NOT EXISTS idx_freg_config      ON file_registry(config_key);
CREATE INDEX IF NOT EXISTS idx_freg_head        ON file_registry(is_head);
CREATE INDEX IF NOT EXISTS idx_freg_format      ON file_registry(format_type);
CREATE INDEX IF NOT EXISTS idx_freg_version_grp ON file_registry(version_group);
COMMIT;
