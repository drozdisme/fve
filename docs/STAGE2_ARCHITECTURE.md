# Stage 2 Architecture — The Extraction Layer

Stage 1 delivered the verification core: given a holed program and a deviation
box, it returns a sound `SAFE / FAIL / ABSTAIN` verdict with a checkable
certificate. Stage 2 builds the layer that turns a source engineering artifact
into exactly that holed program, realizing the full pipeline:

```
xlsx / xlsm / xlsb / image
        |
        v   recognition / parsing            (extractor)
   Excel AST  +  values  +  names
        |
        v   evaluation + perturbation         (oracle)
   Dependency Graph
        |
        v   merge + symbol resolution          (model/sdg)
   Semantic Dependency Graph
        |
        v   function mapping + ambiguity        (model/lower, model/bundle)
   Holed Program  (Stage 1 Nodep)
        |
        v
   Stage 1 Verifier  ->  SAFE / FAIL / ABSTAIN + certificate
```

## 1. Lifting source artifacts

`extractor/zip` reads the OPC container shared by xlsx/xlsm/xlsb. It implements
the ZIP central directory walk and a complete DEFLATE (RFC 1951) inflater —
stored, fixed-Huffman and dynamic-Huffman blocks — so genuine Excel files
decompress with no external library. `extractor/xml` is a small,
namespace-tolerant XML parser used for the OOXML parts.

`extractor/xlsx` reads `workbook.xml` (sheet order, defined names),
`sharedStrings.xml`, the worksheet parts (cell values, `<f>` formulas, inline and
shared strings), and resolves relationships. xlsm is the same container with a
macro-enabled content type and is detected as such. `extractor/xlsb` reads the
binary BIFF12 stream: variable-length record ids and sizes, the cell value
records (real, RK, shared-string-index, string, bool), and a subset of the RPN
(`Rgce`) token language for cached formulas (literals, references, ranges,
binary operators, and common functions), reconstructing an A1 formula string when
the tokens are supported.

`extractor/formula` is the Excel formula front end: a tokenizer that recognizes
numbers, strings, sheet-qualified references and ranges, names, functions and the
full operator set, and a precedence-climbing parser producing an Excel AST. It
also extracts the reference closure of a formula (cells, ranges, names).

## 2. Never commit to a single interpretation

The architecture's recognition rule — never collapse ambiguity prematurely — is
realized by `model/bundle`. A `Bundle` carries competing hypotheses with
confidences and a provenance label, supports normalization and flooring without
ever reducing genuine ambiguity to one branch, and lowers to a Stage 1 node:
one survivor becomes that node, several become a `disj`, and any residual
"unknown" mass injects a `hole`. This is the structural guarantee that unknown
content widens uncertainty rather than inventing precision.

`extractor/ocr` applies the same principle to raster formulas. Embedded images
are extracted from the container directly. A recognition *lattice* (per-position
symbol alternatives with confidences — the artifact an upstream visual model
would emit) is expanded into the top candidate formula strings, each parsed and
lowered, and assembled into a bundle; an explicit unknown token forces the
unknown-widening path. The pixel-to-lattice visual recognizer itself is the
deferred machine-learning component; everything downstream of the lattice is
implemented and tested.

## 3. The oracle as ground truth

`oracle` evaluates the reconstructed workbook deterministically: a formula
interpreter with dependency resolution, memoization, and cycle guarding,
covering arithmetic, comparison, aggregation, lookup-free math functions and
`IF`. This is the Stage 2 stand-in for sandboxed headless execution — it is the
truth against which structure is probed, not a re-implementation of Excel.

`oracle/intervene` performs Interventional Dependency Induction: each input is
perturbed, every output re-evaluated from a cleared cache, and a dependency edge
with a finite-difference sensitivity is recorded wherever a response exceeds
tolerance. Because dependence is detected by *response*, not by reading
formulas, the discovered structure is placement-invariant and survives
indirection.

## 4. The Semantic Dependency Graph

`model/sdg` merges two independent dependency sources: the syntactic references
parsed from formulas and the interventional edges induced by the oracle. Edges
present in both are marked accordingly, giving a confidence signal; edges from
only one source are retained. Symbols are resolved by adjacent-label heuristics
and defined names, and nodes carry an ambiguity flag with an alternative count so
bundle-derived uncertainty is visible in the graph.

## 5. Lowering to a holed program

`model/lower` walks the Excel AST of a selected margin cell and emits a Stage 1
`Nodep`, inlining referenced formula cells as a shared DAG and turning inputs
into variables. Function mapping is explicit: `SUM`/`AVERAGE` fold to
add/divide, `MIN`/`MAX`/`ABS` to the core calls, the elementary functions to
their core operators, `IF` to a `disj` over its branches (a sound branch hull),
and any unmodeled construct — `VLOOKUP`, `INDEX`, ranges in scalar position,
string operators — to a `hole`. The deviation box is built from each input's base
value and a relative tolerance; the resulting program is handed to the Stage 1
engine unchanged.

## 6. Dimensional inference

`model/dim` builds a dimension graph whose constraints are the dimensional
equations implied by the formulas (equality at additions and comparisons, linear
combination at products and powers) and solves the integer system with a Smith
Normal Form solver. SNF yields the determined dimensions, flags underdetermined
symbols (left free, to be treated as unknown), and detects inconsistency
(dimensional clashes). It is the data-free prior `F1` made constructive: a symbol
whose dimension is forced is pinned; one that is not stays open rather than being
guessed.

## 7. Soundness is preserved end to end

Every Stage 2 path that lacks information routes to a wider enclosure: an
unparsed or unmodeled construct becomes a hole, an ambiguous recognition becomes
a disjunction, an underdetermined dimension stays open. None of these can narrow
the `MS` enclosure, so none can manufacture a `SAFE`. The Stage 1 invariant — a
`SAFE` verdict only when a rigorous lower bound proves `MS > 0` — is inherited
unchanged, because Stage 2 only ever produces inputs to the Stage 1 verifier and
never weakens its checks.
