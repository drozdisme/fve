BEGIN;
CREATE TABLE IF NOT EXISTS section_map (
    folder_id INTEGER PRIMARY KEY, section_name TEXT NOT NULL,
    domain TEXT, parent_section INTEGER);
CREATE OR REPLACE VIEW section_status AS
SELECT sm.section_name,
       count(fr.id) AS total_files,
       count(*) FILTER (WHERE t.verdict = 'SAFE')    AS safe_count,
       count(*) FILTER (WHERE t.verdict = 'FAIL')    AS fail_count,
       count(*) FILTER (WHERE t.verdict = 'ABSTAIN') AS abstain_count,
       max(t.cs) AS max_cs, max(fr.mtime) AS last_updated
FROM section_map sm
JOIN folders f       ON f.id = sm.folder_id
JOIN file_registry fr ON fr.folder_id = f.id AND fr.is_head
LEFT JOIN runs r      ON r.artifact_id = fr.id
LEFT JOIN targets t   ON t.run_id = r.id
GROUP BY sm.section_name;
COMMIT;
