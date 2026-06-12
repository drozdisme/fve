BEGIN;
ALTER TABLE file_registry ADD COLUMN IF NOT EXISTS edit_mode TEXT DEFAULT 'excel'; -- 'excel' | 'structured'
COMMIT;
