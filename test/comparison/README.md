# Table size comparison: flex vs lxr

Generates random lexer sets, runs both `flex` and `lxr` on each, and
compares the size of the generated tables. The hand-written grammars in
`languages/*/*.lex` are included too (tables only).

```sh
make                      # 40 random sets (5..40 rules) + languages/*/*.lex
make N=200 SEED=7         # more / other random sets
make MODES=-Ce,-Cem,-Cf   # flex table modes to compare against
make NOCHECK=1            # skip the token stream check
make plot                 # docs/img/flex-comparison-{light,dark}.svg from out/results.csv
make clean
```

Results go to `out/report.md`, `out/results.csv` and `out/cases/*.l`, where
each case can be re-run by hand. Use `python3 compare.py --help` for all
options, e.g. `--lxr <binary>` to test another build.

## What is compared

| | tables counted |
|---|---|
| lxr  | `check next base accept def class` (`lxr_meta` is emitted but unused, so it is **not** counted) |
| flex | `yy_accept yy_ec yy_meta yy_base yy_def yy_nxt yy_chk` (whatever the mode emits) |

* **bytes**: entries × size of the C element type in the generated code
* **chk/nxt**: size of the compressed transition storage
* flex `-Ce` (equivalence classes, no meta classes) is the fair comparison;
  `-Cem` is flex's default.

## Same language check

A size comparison only makes sense if both tools built the same scanner.
Every random set ends with catch-all `.` and `\n` rules, and both scanners
are compiled and run on the same random input. The (token id, length)
streams must be identical (`tokens = same`). Only matching cases are used in
the size summary, and `make` exits with 1 if any case differs.

## Grammar subset

The random sets use only syntax that both tools accept with the same
meaning: literals, escapes, quoted strings, `[..]`, `[^..]`, ranges,
`* + ? | ( )`, `{m}`, `{m,n}`, `{m,}`, a leading `^`, and macros (always
wrapped in `( )` or `[ ]`).

Not used: trailing context (`a/b`), `<<EOF>>`, start conditions, `$` (flex
treats it as trailing context), `\d \w \s`, and grouped rules.

## Skipped cases

A case is skipped (and listed with its reason) when lxr rejects or crashes
on it. Typical reasons:

* *Some tokens are never reachable*: a genuinely unreachable rule
  (flex warns "rule cannot be matched" for these)
* *Table compression: Out of table size limit 32767*: lxr writes `short`
  tables, so `check[]`/`next[]` can't exceed `SHRT_MAX` entries (flex
  switches to 32-bit tables for such DFAs, typically with thousands of
  states)
* *lxr timeout*: a very large DFA; lxr's Hopcroft minimization is much
  slower than flex's at that size (`--timeout` sets the limit, default
  120 s). Generation times are in `results.csv` (`lxr_time`, `flex*_time`).
* *CRASH*: should not happen; it would be an lxr bug
