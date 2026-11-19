# Table size comparison: flex vs lxr

Generates random lexer sets, runs both `flex` and `lxr` on each, and
compares the size of the generated tables. The hand-written grammars in
`languages/*/*.lex` are included too (tables only).

```sh
make                      # 40 random sets (5..40 rules) + languages/*/*.lex
make N=200 SEED=7         # more / other random sets
make MODES=-Ce,-Cem,-Cf   # flex table modes to compare against
make NOCHECK=1            # skip the token stream check
make META=1               # lxr with meta classes (lxr -m)
make plot                 # docs/img/flex-comparison-{light,dark}.svg from out/results.csv
make clean
```

Results go to `out/report.md`, `out/results.csv` and `out/cases/*.l`, where
each case can be re-run by hand. Use `python3 compare.py --help` for all
options, e.g. `--lxr <binary>` to test another build.

## What is compared

| | tables counted |
|---|---|
| lxr  | `check next base accept def class`, and `meta` (emitted only with `lxr -m`) |
| flex | `yy_accept yy_ec yy_meta yy_base yy_def yy_nxt yy_chk` (whatever the mode emits) |

* **bytes**: entries × size of the C element type in the generated code
* **chk/nxt**: size of the compressed transition storage
* lxr (default) against flex `-Ce` (equivalence classes, no meta classes),
  and `lxr -m` against flex `-Cem` (flex's default, with meta classes) are
  the fair comparisons.

## Scanning speed (`make bench`)

`bench.py` compares how fast the generated scanners run. The same grammar
is turned into a scanner by lxr and by flex (`-Cem` default, `-Ce`, and
`-8 -Cf` full tables), all compiled with `gcc -O2`, and each one scans the
same large input:

| grammar | input |
|---|---|
| `languages/c99/c99.lex` | C source: preprocessed system headers + this repository's sources |
| `languages/json/json.lex` | generated JSON (numbers, nested arrays/objects, escaped strings) |
| random lexers (`--random`) | random text from `gen_lex.py` |

Only the token loop is timed, inside the process. `IO=mem` (the default)
reads the input into memory first and measures pure scanning;
`IO=file` lets the scanner read the file itself. Runs are interleaved,
pinned to one CPU with `taskset` when available, and the median is
reported. Every scanner must produce the same token stream as lxr (count
and hash of token ids and lengths); a difference is reported as MISMATCH.

```sh
make bench                       # 32 MB inputs, 5 runs
make bench SIZE=64 RUNS=7 IO=both
make bench META=1                # also lxr -m (meta classes)
```

Results go to `out/bench/bench.md` and `out/bench/bench.csv`.

Note: flex `-Cf` / `-CF` build a 7-bit scanner unless `-8` is given,
which mis-scans bytes >= 0x80 (for example UTF-8 in C comments). That is
why the full-table mode is `-8 -Cf`.

## Same language check

A size comparison only makes sense if both tools built the same scanner.
Every random set ends with catch-all `.` and `\n` rules, and both scanners
are compiled and run on the same random input. The (token id, length)
streams must be identical (`tokens = same`). Only matching cases are used in
the size summary, and `make` exits with 1 if any case differs.

The lxr scanner is also built with a 7-byte buffer (`-DLXR_BUFF_SIZE=7`),
so that buffer refills happen inside almost every token; the default 16 kB
buffer is never refilled by the small inputs. A difference there shows as
`DIFF(buf7)`.

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
