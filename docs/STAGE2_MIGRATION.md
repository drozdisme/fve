# Stage 2 Migration Notes

Stage 2 adds the extraction layer on top of Stage 1. Stage 1 was not redesigned;
its public headers and behaviour are unchanged. Everything new sits in three new
top-level trees and consumes Stage 1 only through its existing interfaces.

## What changed for an existing Stage 1 user

Nothing breaks. The Stage 1 library, the `verify` binary, the certificate format,
the audit chain, and all Stage 1 tests are identical. The build now also compiles
the new sources into the same `libcore.a` and adds a second binary, `extract`.

If you previously built Stage 1:

```
make            # now also builds bin/extract
make test       # now also runs the Stage 2 suite
```

No source under `core/` was modified except that none was: the Stage 2 layers
include `core/` headers (`core/ast/ast.hpp`, `core/engine.hpp`,
`core/types/types.hpp`, `core/interval/ival.hpp`) and call them as-is.

## New directory trees

```
extractor/
  zip/       ZIP container reader + DEFLATE inflate (no external deps)
  xml/       minimal XML parser
  formula/   Excel formula tokenizer + parser -> Excel AST, dependency extraction
  xlsx/      xlsx / xlsm reader (sheets, cells, formulas, shared strings, names)
  xlsb/      xlsb (BIFF12) record reader + RK + RPN formula decode
  ocr/       embedded image extraction + recognition-lattice -> candidate bundle
  pipeline.* file -> model -> SDG -> holed program -> Stage 1 verdict
oracle/
  oracle.*    deterministic workbook evaluator (the sandboxed-execution stand-in)
  intervene.* interventional dependency induction (perturbation -> response)
model/
  workbook/  in-memory workbook model + A1 reference utilities
  bundle/    candidate AST bundles (ambiguity preservation)
  dim/       dimension graph + integer Smith Normal Form solver + inference
  sdg/       Semantic Dependency Graph builder (syntactic + interventional merge)
  lower/     Excel AST -> Stage 1 holed program (Nodep)
cmd/extract.cpp   Stage 2 CLI
tests/fixtures/   generated sample workbooks + generator (gen.py)
```

## Build system

`Makefile` `SRC` now globs `extractor/*.cpp extractor/*/*.cpp oracle/*.cpp
model/*/*.cpp` in addition to `core`. A new `bin/extract` target links
`cmd/extract.cpp` against `libcore.a`. `CMakeLists.txt` mirrors this and adds the
`extract` executable.

There are still zero external dependencies. ZIP inflate, XML, the Excel parser,
the xlsb record decoder, and the SNF solver are all implemented in-repo and
compile offline with g++ and C++17.

## Regenerating fixtures

The committed fixtures under `tests/fixtures/` are produced by `gen.py`
(requires Python with `openpyxl`, used only at fixture-generation time, never at
build or test time):

```
python3 tests/fixtures/gen.py
```

This writes `holed.xlsx`, `img.xlsx`, and the hand-crafted `mini.xlsb`. The base
`beam.xlsx` and `macro.xlsm` are also committed. Tests read these files directly.

## New CLI

```
bin/extract <file.xlsx|xlsm|xlsb> [rel_dev]
```

Loads the workbook, induces the dependency graph by intervention, builds the SDG,
selects margin (sink) cells, lowers each to a Stage 1 holed program, runs the
Stage 1 verifier under a relative deviation box (`rel_dev`, default 0.05), and
prints a JSON report with per-target verdict, MS enclosure, hole count, and a
validated certificate reference, plus the audit-chain status.
