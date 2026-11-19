#!/usr/bin/env python3
"""
Scanning speed benchmark : lxr vs flex.

For each grammar, the same .l file is turned into a scanner by lxr and
by flex (one per --modes entry), all compiled with the same compiler and
flags. Each scanner runs over the same large input; only the token loop

    while ( (t = yylex ()) ) { count & hash the token }

is timed, inside the process (no process start-up / compile time).

  --io mem  : the input is read into memory first (lxr_read_bytes () /
              yy_scan_bytes (), not timed). Measures pure scanning.
  --io file : the scanner reads the file itself (lxr_source () / yyin).

Every scanner must produce the same token stream as lxr (token count and
a hash of (token id, yyleng)), otherwise the result is marked MISMATCH.

Grammars : languages/c99/c99.lex (input : C source, i.e. preprocessed
system headers + this repository's sources), languages/json/json.lex
(input : generated JSON), and --random random lexers from gen_lex.py
(input : random text).

  python3 bench.py                       # defaults, writes out/bench/
  python3 bench.py --size 64 --runs 7    # 64 MB inputs, 7 runs each
"""

import argparse
import csv
import glob
import json
import os
import random
import re
import shutil
import statistics
import subprocess
import sys

import gen_lex

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

# Prepended to every grammar : flex options, and lxr names mapped to flex
COMPAT = r"""%option noyywrap
%{
#ifdef FLEX_SCANNER
  #define lxr_input() input()
#endif
%}
"""

# Replaces the user code section of every grammar
DRIVER = r"""
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double bench_now (void) {
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double) t.tv_sec + 1e-9 * (double) t.tv_nsec;
}

int main (int argc, char ** argv) {
  if (argc < 3) {
    fprintf (stderr, "usage: %s input mem|file\n", argv[0]);
    return 2;
  }
  int mem = (argv[2][0] == 'm');
  char * buf = NULL;
  long len = 0;
  if (mem) {
    FILE * f = fopen (argv[1], "rb");
    if (!f) return 3;
    fseek (f, 0, SEEK_END); len = ftell (f); rewind (f);
    buf = malloc ((size_t) len + 2);
    if (fread (buf, 1, (size_t) len, f) != (size_t) len) return 4;
    buf [len] = buf [len + 1] = '\0';
    fclose (f);
  }
#ifdef FLEX_SCANNER
  if (mem) yy_scan_bytes (buf, (int) len);
  else if ( !(yyin = fopen (argv[1], "rb")) ) return 3;
#else
  if (mem) lxr_read_bytes (buf, (size_t) len, 1);
  else lxr_source (argv[1]);
#endif
  unsigned long n = 0, h = 1469598103934665603UL;
  int t;
  if (argc > 3) {                   /* debug : dump "offset id length" */
    unsigned long off = 0;
    while ( (t = BENCH_LEXFN ()) ) {
      printf ("%lu %d %d\n", off, t, (int) yyleng);
      off += (unsigned long) yyleng;
    }
    return 0;
  }
  double t0 = bench_now ();
  while ( (t = BENCH_LEXFN ()) ) {
    n++;
    h = (h ^ (unsigned long) (t * 4099 + yyleng)) * 1099511628211UL;
  }
  double t1 = bench_now ();
  printf ("%lu %016lx %.6f\n", n, h, t1 - t0);
  return 0;
}
"""


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, **kw)


def split_sections(text):
    """Return (definitions, rules) of a .l file (user code dropped)."""
    lines = text.split("\n")
    marks = [i for i, l in enumerate(lines) if l.startswith("%%")]
    if len(marks) < 1:
        raise ValueError("no %% found")
    end = marks[1] if len(marks) > 1 else len(lines)
    return "\n".join(lines[:marks[0]]), "\n".join(lines[marks[0] + 1:end])


def bench_grammar(text):
    """A copy of the grammar with the compat block and the driver."""
    defs, rules = split_sections(text)
    fn = "yylex" if "YY_DECL" in defs else "lxr_lex"
    # lxr reads the flex options (%option ..) only as a warning
    return (COMPAT + defs + "\n%%\n" + rules + "\n%%\n"
            + "#ifdef FLEX_SCANNER\n  #define BENCH_LEXFN yylex\n#else\n"
            + "  #define BENCH_LEXFN %s\n#endif\n" % fn + DRIVER)


# ------------------------------------------------------------- inputs

def c_input(path, size, work):
    """C source : preprocessed system headers + repository sources."""
    hdr = os.path.join(work, "headers.c")
    with open(hdr, "w") as f:
        for h in ("stdio.h", "stdlib.h", "string.h", "math.h", "ctype.h",
                  "errno.h", "limits.h", "stdint.h", "time.h", "signal.h",
                  "locale.h", "wchar.h", "assert.h", "stdarg.h", "setjmp.h"):
            f.write("#include <%s>\n" % h)
    p = run(["gcc", "-E", "-P", hdr])
    chunks = [p.stdout.decode(errors="replace")] if p.returncode == 0 else []
    for src in sorted(glob.glob(os.path.join(ROOT, "src", "*.c"))) + \
            [os.path.join(ROOT, "languages", "c99", "sample.c")]:
        chunks.append(open(src, errors="replace").read())
    unit = "\n".join(chunks)
    write_repeated(path, unit, size)


def json_input(path, size, seed):
    rng = random.Random(seed)
    words = ["alpha", "beta", "gamma", "delta", "lexer", "token", "state",
             "café", "naïve", "quote\"d", "back\\slash", "tab\tbed"]

    def value(depth):
        r = rng.random()
        if depth > 3 or r < 0.35:
            k = rng.random()
            if k < 0.3:
                return rng.randint(-10 ** 6, 10 ** 6)
            if k < 0.5:
                return round(rng.uniform(-1e4, 1e4), rng.randint(0, 6))
            if k < 0.6:
                return rng.choice([True, False, None])
            return " ".join(rng.choice(words) for _ in range(rng.randint(1, 5)))
        if r < 0.65:
            return [value(depth + 1) for _ in range(rng.randint(0, 6))]
        return {"%s_%d" % (rng.choice(words), i): value(depth + 1)
                for i in range(rng.randint(0, 6))}

    parts, n = [], 0
    while n < 1 << 20:                                  # ~1 MB unit
        s = json.dumps(value(0), indent=1)
        parts.append(s)
        n += len(s) + 2
    write_repeated(path, "[\n" + ",\n".join(parts) + "\n]\n", size,
                   joiner=None)


def write_repeated(path, unit, size, joiner="\n"):
    """Write 'unit' repeatedly until 'size' bytes (whole units only)."""
    data = unit.encode()
    with open(path, "wb") as f:
        if joiner is None:
            # JSON : one top level array of many copies
            body = data.strip()[1:-1].strip()          # remove [ ]
            f.write(b"[\n")
            written = 2
            first = True
            while written < size:
                if not first:
                    f.write(b",\n")
                f.write(body)
                written += len(body) + 2
                first = False
            f.write(b"\n]\n")
        else:
            written = 0
            while written < size:
                f.write(data + joiner.encode())
                written += len(data) + 1


def random_case(i, seed, size, work, lxr):
    """A random lexer that lxr accepts (random sets often contain rules
    that can never match, which lxr rejects : try the next candidate)."""
    probe = os.path.join(work, "probe.l")
    for k in range(100):
        rng = random.Random((seed * 7919 + i) * 1000 + k)
        nrules = rng.randint(10, 40)
        text = gen_lex.generate(rng, nrules)
        with open(probe, "w") as f:
            f.write(text)
        p = run([lxr, "-o", os.devnull, probe], timeout=120)
        if p.returncode == 0:
            break
    else:
        raise RuntimeError("no random lexer accepted by lxr in 100 tries")
    inp = os.path.join(work, "rand_%d.txt" % i)
    unit = gen_lex.random_input(rng, 1 << 18)          # 256 kB unit
    with open(inp, "wb") as f:
        n = 0
        while n < size:
            f.write(unit)
            n += len(unit)
    return "rand_%d" % i, text, inp, "random lexer (%d rules)" % nrules


# ------------------------------------------------------------- build

def build(name, text, work, lxr, flex, modes, cc, cflags, incdirs):
    """Return { scanner : exe } ; scanners that fail to build are missing."""
    lfile = os.path.join(work, name + ".l")
    with open(lfile, "w") as f:
        f.write(bench_grammar(text))
    inc = sum((["-I", d] for d in incdirs), [])
    exes, errors = {}, {}
    gens = [("lxr", [lxr, "-o", os.path.join(work, name + ".lxr.c"), lfile],
             os.path.join(work, name + ".lxr.c"))]
    for m in modes:
        c = os.path.join(work, "%s.flex%s.c" % (name, m.replace(" ", "")))
        gens.append(("flex " + m, [flex] + m.split() + ["-o", c, lfile], c))
    for scanner, cmd, c in gens:
        p = run(cmd)
        if p.returncode or not os.path.exists(c):
            errors[scanner] = "generate failed: " + \
                (p.stdout + p.stderr).decode(errors="replace").strip()[-200:]
            continue
        exe = c[:-2]
        p = run([cc] + cflags + inc + ["-o", exe, c])
        if p.returncode:
            errors[scanner] = "compile failed: " + p.stderr.decode(errors="replace")[:200]
            continue
        exes[scanner] = exe
    return exes, errors


def measure(exes, inp, io, runs, pin):
    """Interleaved runs. Returns { scanner : (tokens, hash, [times]) }."""
    res = {s: [None, None, []] for s in exes}
    prefix = ["taskset", "-c", pin] if pin and shutil.which("taskset") else []
    order = list(exes)
    for r in range(runs):
        for s in (order if r % 2 == 0 else order[::-1]):
            p = run(prefix + [exes[s], inp, io], timeout=600)
            out = p.stdout.decode().split()
            if p.returncode or len(out) != 3:
                res[s][0] = "CRASH rc=%d %s" % (p.returncode,
                                                p.stderr.decode(errors="replace")[:80])
                continue
            n, h, t = int(out[0]), out[1], float(out[2])
            if res[s][0] is None:
                res[s][0], res[s][1] = n, h
            elif (res[s][0], res[s][1]) != (n, h):
                res[s][0] = "NONDETERMINISTIC"
            res[s][2].append(t)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lxr", default=os.path.join(ROOT, "lxr"))
    ap.add_argument("--flex", default="flex")
    # Note : -Cf / -CF make a 7-bit scanner unless -8 is given, which
    # mis-scans bytes >= 0x80 (e.g. UTF-8 in C comments and strings)
    ap.add_argument("--modes", default="-Cem,-Ce,-8 -Cf",
                    help="flex table modes (a mode may hold several options, "
                         "e.g. '-8 -Cf'); the first is the baseline (flex default)")
    ap.add_argument("--size", type=int, default=32, help="input size in MB")
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--io", default="mem", choices=["mem", "file", "both"])
    ap.add_argument("--random", type=int, default=3, help="number of random lexers")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--cc", default="gcc")
    ap.add_argument("--cflags", default="-O2 -w -D_GNU_SOURCE")
    ap.add_argument("--pin", default="2", help="CPU to pin to with taskset ('' : none)")
    ap.add_argument("--out", default=os.path.join(HERE, "out", "bench"))
    args = ap.parse_args()

    if shutil.which(args.flex) is None:
        sys.exit("flex not found")
    if not os.path.exists(args.lxr):
        sys.exit("lxr not found (%s). Run 'make lxr' in the repo root" % args.lxr)
    modes = [m.strip() for m in args.modes.split(",") if m.strip()]
    ios = ["mem", "file"] if args.io == "both" else [args.io]
    size = args.size << 20
    work = args.out
    os.makedirs(work, exist_ok=True)
    flexver = run([args.flex, "--version"]).stdout.decode().split()[-1]
    ccver = run([args.cc, "--version"]).stdout.decode().splitlines()[0]

    # grammars : (name, .l text, input file, description, include dirs)
    cases = []
    c_in = os.path.join(work, "c99_input.c")
    if not os.path.exists(c_in) or os.path.getsize(c_in) < size:
        c_input(c_in, size, work)
    cases.append(("c99", open(os.path.join(ROOT, "languages/c99/c99.lex")).read(),
                  c_in, "C source", [os.path.join(ROOT, "languages/c99")]))
    j_in = os.path.join(work, "json_input.json")
    if not os.path.exists(j_in) or os.path.getsize(j_in) < size:
        json_input(j_in, size, args.seed)
    cases.append(("json", open(os.path.join(ROOT, "languages/json/json.lex")).read(),
                  j_in, "JSON", [os.path.join(ROOT, "languages/json")]))
    for i in range(args.random):
        name, text, inp, desc = random_case(i, args.seed, size, work, args.lxr)
        cases.append((name, text, inp, desc, []))

    rows, problems = [], []
    scanners = ["lxr"] + ["flex " + m for m in modes]
    base = "flex " + modes[0]
    for name, text, inp, desc, incs in cases:
        exes, errors = build(name, text, work, args.lxr, args.flex, modes,
                             args.cc, args.cflags.split(), incs)
        for s, e in errors.items():
            problems.append("%s / %s : %s" % (name, s, e))
        if "lxr" not in exes:
            continue
        mb = os.path.getsize(inp) / 1e6
        for io in ios:
            res = measure(exes, inp, io, args.runs, args.pin)
            ref = res["lxr"]
            row = {"grammar": name, "input": desc, "io": io, "MB": round(mb, 1),
                   "tokens": ref[0]}
            for s in scanners:
                if s not in res:
                    row[s + " s"] = row[s + " MB/s"] = "-"
                    continue
                n, h, ts = res[s]
                ok = isinstance(n, int) and (n, h) == (ref[0], ref[1])
                if not ok:
                    problems.append("%s / %s / %s : %s" % (
                        name, io, s, n if not isinstance(n, int)
                        else "MISMATCH (tokens %s vs lxr %s)" % (n, ref[0])))
                med = statistics.median(ts) if ts else float("nan")
                row[s + " s"] = round(med, 4)
                row[s + " MB/s"] = round(mb / med, 1) if ts else "-"
                row[s + " ok"] = "ok" if ok else "MISMATCH"
            rows.append(row)
            print("%-8s %-4s %s" % (name, io, "  ".join(
                "%s %s MB/s" % (s, row.get(s + " MB/s")) for s in scanners)), flush=True)

    # ------------------------------------------------------------ report
    keys = ["grammar", "input", "io", "MB", "tokens"]
    for s in scanners:
        keys += [s + " s", s + " MB/s", s + " ok"]
    with open(os.path.join(work, "bench.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)

    head = ["grammar", "io", "input MB", "tokens"] + \
        ["%s MB/s" % s for s in scanners] + ["lxr / %s time" % base]
    lines = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for r in rows:
        cells = [r["grammar"], r["io"], str(r["MB"]), str(r["tokens"])]
        for s in scanners:
            v = r.get(s + " MB/s", "-")
            if r.get(s + " ok") == "MISMATCH":
                v = "%s (MISMATCH)" % v
            cells.append(str(v))
        lt, bt = r.get("lxr s"), r.get(base + " s")
        cells.append("%.2f" % (lt / bt) if isinstance(lt, float) and
                     isinstance(bt, float) and bt > 0 else "-")
        lines.append("| " + " | ".join(cells) + " |")
    notes = [
        "",
        "flex %s, %s, CFLAGS `%s`, median of %d runs%s." % (
            flexver, ccver, args.cflags, args.runs,
            ", pinned to CPU %s" % args.pin if args.pin and shutil.which("taskset") else ""),
        "Only the token loop is timed. io=mem: input already in memory; "
        "io=file: the scanner reads the file.",
        "lxr / %s time > 1 means lxr is slower." % base,
    ]
    if problems:
        notes += ["", "Problems:"] + ["  - " + p for p in problems]
    report = "\n".join(lines + notes) + "\n"
    with open(os.path.join(work, "bench.md"), "w") as f:
        f.write(report)
    print()
    print(report)
    print("written : %s/{bench.md,bench.csv}" % work)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
