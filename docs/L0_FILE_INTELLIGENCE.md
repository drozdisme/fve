# L0 — File Intelligence Layer

L0 is Ярус 0 of the architecture: the foundation that turns a chaotic tree of
engineering spreadsheets into a structured, versioned, queryable registry while
**preserving the folder hierarchy** the files live in. Without it the verification
core has nothing to operate on at scale; with it the platform can crawl a real
filesystem, classify every file, resolve the live version of each configuration,
and migrate the whole picture into the database.

## Pipeline

```
filesystem root
  -> Crawler          recursive walk, folder tree + file entries
  -> Fingerprinter    format classification (domain markers) + structural signature
  -> Version Resolver  config grouping + is_head selection (revision/mtime/depth)
  -> Registry          FileEntry + Folder records
  -> Migration         folders + file_registry rows (Store + PostgreSQL)
```

## Crawler (`l0/crawler`)

Recursive directory walk producing a folder tree and file list. Each folder
carries `parent`, `depth`, and its path relative to the crawl root; each file
carries its relative path, size, and mtime. Incremental crawls are supported via
a `since_mtime` cutoff (full on first run, changed-only afterwards). Hidden files
and non-target extensions are skipped; the default target set is
`xlsx, xlsm, xlsb, csv`.

## Fingerprinter (`l0/fingerprint`)

Classifies each file against the **Domain Schema Registry** — a table of formats
(stringer FEM loads, rivet joint, bolt joint, frame section, panel buckling, …),
each with key markers (`BAY ID`, `FEM LOAD CASE`, `BACR15FV`, `MSstr_buck`, …).
For spreadsheets the file is parsed with the Stage 2 reader and every cell label,
formula, and named range is scanned for markers; for CSV/TXT the first 4 KB are
scanned. The best-matching schema wins; no marker match yields
`unknown_format` (queued for manual labelling, never a crash).

Beyond markers it computes a **structural signature**: a hash over the sheet
layout, the set of functions used, and the named ranges — the formula *skeleton*,
not the values. Files with the same structure cluster to the same signature even
when their numbers differ, which is what lets one template serve many files.

## Version Resolver (`l0/version`)

Parses revision tokens from filenames (`Rev_D`, `RevC`, `v2`, `ver10`, `R3`),
yielding a `config_key` (the name with the revision stripped) and a numeric rank
(`D > C`, `v10 > v2`). Files sharing a `config_key` form a version group; within a
group the head is chosen by **rank, then mtime, then folder depth** — the
architecture's rule that the latest revision, most recently modified, closest to
the final assembly is the live one. Exactly one file per configuration is marked
`is_head`.

## Registry & migration (`l0/registry`)

`build_registry(root)` runs the whole pipeline and returns a `Catalog` of
`Folder` and `FileEntry` records. `migrate(catalog, store)` persists it:

- every folder becomes a row keyed by id with `parent_id`, `depth`, `path`, `rel`
  — the directory tree is reconstructable from the `parent_id` links;
- every file becomes a `file_registry` row with `folder_id`, `rel_path`,
  `sha256`, `format_type`, `domain`, `structural_sig`, `config_key`, `revision`,
  `rev_rank`, `is_head`, `version_group`, and `version_index`.

`registry_sql(catalog)` emits the equivalent PostgreSQL `INSERT` statements for
the `folders` and `file_registry` tables (schema in
`db/migrations/0004_file_registry.sql`), so the same catalog migrates into a
managed database. **The folder structure is first-class in the schema**:
`folders` is the tree, `file_registry.folder_id` + `rel_path` anchor every file
to its place in it.

## CLI

```
bin/crawl <root_dir> [data_dir] [--sql out.sql]
```

Crawls the tree, migrates into the store at `data_dir`, optionally writes the SQL
migration, and prints a JSON summary (folder/file/head counts, format histogram).

## API

```
POST /api/l0/crawl            {"root": "..."}   crawl + migrate, return summary
GET  /api/l0/folders                            the folder tree (preserves hierarchy)
GET  /api/l0/files[?folder=N]                   registry entries, optionally by folder
POST /api/l0/files/:id/verify?rel=..            verify a registered file end-to-end
```

`verify_registered` reads the file from its recorded path, ingests it as an
artifact, links provenance (`file -[registered_as]-> artifact`), and runs the full
verification pipeline — closing the loop from a raw file on disk to a
`SAFE / FAIL / ABSTAIN` verdict with a certificate.

## What this closes

The architecture names L0 as the resolution to Кризис 1 (data chaos): ~2M files,
~20 formats, arbitrary directory structure, no registry, no notion of which
version is live. L0 delivers the live registry, the format classification, the
version resolver, and the migration — with the directory structure preserved end
to end, exactly as required.

## Real-data hardening

L0 is built to survive a real 2M-file estate, validated against a real 184 MB
`.xlsb` (777P2F skin-stringer template, worksheet parts decompressing to 200 MB+):

- **Bounded fingerprinting.** A seek-based ZIP reader (`open_zip_file`) parses only
  the central directory and inflates single small parts on demand. Classification
  reads `workbook` (sheet names + named ranges) and `sharedStrings`, plus *small*
  worksheet parts as an inline-string fallback, under a byte budget — the giant
  sheets are never decompressed. The real file classifies in ~100 ms under a
  300 MB memory cap.
- **Streaming hashing.** `sha256_file` hashes in 64 KB chunks; peak memory is
  constant regardless of file size (the 184 MB file hashes without loading).
- **Cycle & symlink safety.** Directory traversal tracks visited `(dev,inode)`
  pairs and a depth guard, so symlink loops are detected and recorded
  (`cycle_skipped`) instead of hanging.
- **Error capture & triage queues.** Unreadable directories, dangling symlinks,
  and depth-exceeded paths are recorded in an `l0_errors` log; files that match no
  schema go to the `unknown_format` queue for manual labelling. Nothing is
  silently dropped and no single bad file aborts the crawl.
- **Real revision tokens.** `parse_revision` handles letter+digit revisions like
  `RevG1` (rank 7.001, ordered `RevG < RevG1 < RevG2 < RevH`) without false
  positives on words like `Revised`.

## One word → newest version

The headline query path. `Service::search(word)` (CLI `bin/find <word>`, API
`GET /api/l0/search?q=`) matches a single word against each configuration's
`config_key`, name, path, `format_type`, `domain`, matched markers, and revision,
returns only `is_head` entries (the newest version of each configuration), ranked
by config-key match strength, marker hits, and mtime, and reports the single
`newest` result.

On the real file an engineer typing `stringer`, `655`, `buckling`, `SnS`, or
`skin` resolves directly to the newest revision of the 777P2F skin-stringer
template — exactly the "type one word, get the live version" workflow.

## Reading every cell (streaming, no data skipped)

Classification reads only metadata, but **no data is discarded** — when the full
contents are needed (analysis, audit, export), a streaming path reads every cell
of arbitrarily large sheets with bounded memory:

- **Streaming inflate** (`inflate_cb`): DEFLATE with a 32 KB sliding window that
  emits output through a callback. The decompressed stream is never materialized,
  so a worksheet part that expands to 200 MB+ costs ~32 KB of window, not 200 MB.
- **Streaming BIFF12 assembler** (`stream_part_records`): reassembles records
  across inflate chunks, keeping only the current partial record, and emits each
  record to a callback.
- **Whole-workbook scan** (`stream_workbook_stats`): walks every worksheet part
  and visits every cell record.

Validated on the real 184 MB `.xlsb`: all 20 sheets, **50.96 M records / 50.79 M
cells (701 MB uncompressed)** read in ~10 s under a **250 MB memory cap** — every
cell, nothing skipped, on a machine that could never hold the file uncompressed.
