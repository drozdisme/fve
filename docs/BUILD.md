# Build and Run

## Requirements

- A C++17 compiler. Tested with `g++ 13.3`.
- GNU Make, or CMake >= 3.10.
- No external libraries. SHA-256, JSON, and the test harness are implemented in
  the repository.

The build uses `-frounding-math -ffp-contract=off`. These flags are required:
they keep the compiler from constant-folding across rounding-mode changes and
from contracting multiply-add pairs, either of which would break the rigour of
the interval arithmetic.

## Make

```
make            # libcore.a + bin/verify
make test       # build and run the full test suite
make coverage   # instrumented build + run; emits .gcov data
make clean
```

`make test` prints a line per failing check (none expected) followed by a
summary `checks: N   fails: 0` and `ALL PASS`.

## CMake

```
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Binaries land in `build/` (`build/verify`, `build/run_tests`).

## Running the verifier

```
bin/verify examples/safe_margin.json
```

Input is a JSON object:

```json
{
  "ks": 0.0,
  "box": { "L": [90, 110], "sall": [300, 300], "c": [1, 1] },
  "ast": { "k": "op", "op": "sub", "args": [ ... ] }
}
```

- `box` maps each variable to its deviation interval `[lo, hi]`.
- `ks` is an optional safety factor added to the residual.
- `ast` is the expression graph. Node kinds:
  - `{"k":"const","v":<number>}`
  - `{"k":"var","sym":"<name>","phys":[lo,hi]}` (`phys` optional)
  - `{"k":"op","op":"add|sub|mul|div|neg|pow|sqrt|log|exp|sin|cos|tan","args":[...]}`
  - `{"k":"call","fn":"max|min|abs","args":[...]}`
  - `{"k":"hole","id":"<id>","arity":<n>,"regime":"A|B","phys":[lo,hi],"args":[...]}`
  - `{"k":"disj","cands":[...]}`

Output is a JSON report containing the verdict, the `MS` enclosure, the residual,
the certificate reference, the audit leaf, and the full per-node derivation,
followed by `cert_valid` and `audit_ok` flags. The process exit code is `0` for
`SAFE`, `1` for `FAIL`, `3` for `ABSTAIN`.

## Measuring coverage

`make coverage` compiles the core and tests with `--coverage` and runs them.
Per-file line coverage:

```
for f in core/*.cpp core/*/*.cpp; do
  gcov -n -o "$(dirname "$f")" "$f" | grep -A1 "File '$f'"
done
```

Aggregate core line coverage of the included suite is about 91%.
