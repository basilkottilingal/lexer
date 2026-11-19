#!/usr/bin/env python3
"""
Table size comparison : flex vs lxr (this lexer generator).

For each random lexer set (see gen_lex.py) and for the grammars given
with --extra (default : ../../languages/*/*.lex), the script runs

  lxr  <--lxr-flags> -o case.lxr.c  case.l
  flex -v <MODE> -o case.c  case.l      for each MODE in --modes

and parses the generated tables.

  lxr  tables : lxr_check, lxr_next, lxr_base, lxr_accept, lxr_def,
                lxr_class, and lxr_meta (only emitted with meta
                classes, "--lxr-flags=-m").
  flex tables : yy_accept, yy_ec, yy_meta, yy_base, yy_def, yy_nxt,
                yy_chk (whatever the MODE generates).

Sizes are reported as "transition entries" (len of check/next; the
compressed transition storage) and as bytes (entries x element size of
the C type used in the generated code).

Fair comparison : flex "-Ce" (equivalence classes, no meta classes) is
the closest to lxr. "-Cem" is flex's default (adds meta classes).
"-Cf" (full, uncompressed tables) can be added for reference.

With --check (default), both scanners are compiled and run on random
input, and their token streams are compared, to make sure that both
tools built the same language (a table size comparison is meaningless
otherwise).
"""

import argparse
import csv
import glob
import math
import os
import random
import re
import shutil
import statistics
import subprocess
import sys
import time
import zlib

import gen_lex

HERE = os.path.dirname(os.path.abspath(__file__))

ELEM_SIZE = {
    "short": 2, "flex_int16_t": 2, "flex_int32_t": 4, "int": 4,
    "unsigned char": 1, "char": 1, "flex_uint8_t": 1, "YY_CHAR": 1,
}
LXR_TABLES = ["check", "next", "base", "accept", "def", "class", "meta"]
LXR_FLAGS = []         # set from --lxr-flags
FLEX_TABLES = ["yy_accept", "yy_ec", "yy_meta", "yy_base", "yy_def",
               "yy_nxt", "yy_chk"]


TIMEOUT = 120          # seconds per command, set from --timeout


def run(cmd, **kw):
    """subprocess.run with a timeout. Returns None on timeout, else the
    CompletedProcess with an extra attribute 'elapsed' (seconds)."""
    t0 = time.perf_counter()
    try:
        p = subprocess.run(cmd, capture_output=True, timeout=TIMEOUT, **kw)
    except subprocess.TimeoutExpired:
        return None
    p.elapsed = time.perf_counter() - t0
    return p


def parse_arrays(src, prefix):
    """Return { name : (ctype, nentries) } for every static array named
    prefix* with an initializer. Entries are counted from the
    initializer, so multi dimensional arrays (flex -Cf) work too."""
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    out = {}
    decl = re.compile(r"static\s+(?:const\s+)?([A-Za-z_][\w ]*?)\s+("
                      + prefix + r"\w+)\s*((?:\[\s*\d*\s*\])+)\s*=\s*\{")
    for m in decl.finditer(src):
        depth, i = 1, m.end()
        while depth and i < len(src):
            depth += {"{": 1, "}": -1}.get(src[i], 0)
            i += 1
        values = [int(x) for x in re.findall(r"-?\d+", src[m.end():i - 1])]
        out[m.group(2)] = (m.group(1).strip(), len(values), values)
    return out


def nbytes(arrays, names, ychar=1):
    total = 0
    for n in names:
        if n in arrays:
            t, k, _ = arrays[n]
            size = ychar if t == "YY_CHAR" else ELEM_SIZE.get(t, 4)
            total += size * k
    return total


def first_error(text):
    for line in text.splitlines():
        line = line.strip()
        if line and not line.startswith(("warning", "stats", "no:", "table")):
            return line[:90]
    return "?"


def lxr_measure(lxr, lfile, cfile):
    p = run([lxr] + LXR_FLAGS + ["-o", cfile, lfile])
    if p is None:
        return None, "lxr timeout (> %d s)" % TIMEOUT
    out = (p.stdout + p.stderr).decode(errors="replace")
    if p.returncode < 0 or p.returncode == 134:
        return None, "lxr: CRASH (signal %d) %s" % (
            -p.returncode if p.returncode < 0 else 6,
            "allocator assert s <= PAGE_SIZE" if "PAGE_SIZE" in out else "")
    if p.returncode != 0 or not os.path.exists(cfile):
        # lxr prints the errors most recent first : root cause is last
        errs = out.split("failed creating lexer code")[-1].strip().splitlines()
        errs = [e.strip() for e in errs if e.strip()]
        return None, "lxr: " + (" ".join(errs[-2:])[:100] if errs else "?")
    src = open(cfile, errors="replace").read()
    arr = {k[4:]: v for k, v in parse_arrays(src, "lxr_").items()}
    m = re.search(r"no: of states\s+(\d+)", out)
    c = re.search(r"#define\s+lxr_nclass\s+(\d+)", src)
    return {
        "states": int(m.group(1)) if m else -1,
        "classes": int(c.group(1)) - 4 if c else -1,   # minus EOB/EOL/EOF/BOL
        "trans": arr.get("check", ("", 0, []))[1],
        "bytes": nbytes(arr, LXR_TABLES),
        "time": p.elapsed,
    }, None


def flex_measure(flex, mode, lfile, cfile):
    p = run([flex, "-v", mode, "-o", cfile, lfile])
    if p is None:
        return None, "flex timeout"
    err = p.stderr.decode(errors="replace")
    if p.returncode != 0:
        return None, "flex: " + first_error(err)
    src = open(cfile, errors="replace").read()
    arr = parse_arrays(src, "yy_")
    ychar = 2 if re.search(r"typedef\s+flex_(u)?int16_t\s+YY_CHAR", src) else 1
    m = re.search(r"(\d+)/\d+ DFA states", err)
    ec = arr.get("yy_ec")
    meta = arr.get("yy_meta")
    return {
        "states": int(m.group(1)) if m else -1,
        "classes": max(ec[2]) if ec else -1,    # yy_ec values are 1..n
        "meta": meta[1] - 1 if meta else 0,     # yy_meta[0] is unused
        "trans": arr.get("yy_nxt", ("", 0, []))[1],
        "bytes": nbytes(arr, FLEX_TABLES, ychar),
        "time": p.elapsed,
        "warn": "cannot be matched" in err,
    }, None


def check_same(lxr_c, flex_c, workdir, rng, size):
    """Compile both scanners, run them on the same random input and
    compare (token id, token length) streams."""
    """The lxr scanner is also built with a tiny buffer (LXR_BUFF_SIZE=7),
    so that buffer refills happen inside almost every token : the default
    16 kB buffer is never refilled by the small random inputs."""
    exe_l = os.path.join(workdir, "l.out")
    exe_s = os.path.join(workdir, "s.out")
    exe_f = os.path.join(workdir, "f.out")
    for c, e, extra in ((lxr_c, exe_l, []), (lxr_c, exe_s, ["-DLXR_BUFF_SIZE=7"]),
                        (flex_c, exe_f, [])):
        p = run(["gcc", "-w", "-O1"] + extra + ["-o", e, c])
        if p is None or p.returncode:
            return "compile-fail"
    data = gen_lex.random_input(rng, size)
    a, s, b = (run([e], input=data) for e in (exe_l, exe_s, exe_f))
    if a is None or s is None or b is None:
        return "timeout"
    if a.returncode != b.returncode:
        return "rc-differs"
    if a.stdout != b.stdout:
        return "DIFF"
    if s.returncode != b.returncode or s.stdout != b.stdout:
        return "DIFF(buf7)"
    return "same"


def geomean(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else float("nan")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lxr", default=os.path.join(HERE, "..", "..", "lxr"))
    ap.add_argument("--flex", default="flex")
    ap.add_argument("--n", type=int, default=40, help="number of random lexer sets")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--min-rules", type=int, default=5)
    ap.add_argument("--max-rules", type=int, default=40)
    ap.add_argument("--modes", default="-Ce,-Cem",
                    help="comma separated flex table modes (first one is used for --check)")
    ap.add_argument("--extra", nargs="*",
                    default=sorted(
                        f for f in glob.glob(os.path.join(HERE, "..", "..",
                                                          "languages", "*", "*.lex"))
                        if not os.path.basename(f).startswith("error")),
                    help="additional (hand written) grammars, tables only")
    ap.add_argument("--no-check", action="store_true",
                    help="don't compare token streams of the two scanners")
    ap.add_argument("--input-size", type=int, default=4000)
    ap.add_argument("--timeout", type=int, default=120,
                    help="seconds allowed for each lxr / flex run")
    ap.add_argument("--lxr-flags", default="",
                    help="extra lxr flags, ex : --lxr-flags=-m (meta classes)")
    ap.add_argument("--out", default=os.path.join(HERE, "out"))
    ap.add_argument("--keep-c", action="store_true", help="keep generated .c files")
    args = ap.parse_args()
    global TIMEOUT, LXR_FLAGS
    TIMEOUT = args.timeout
    LXR_FLAGS = args.lxr_flags.split()

    if shutil.which(args.flex) is None:
        sys.exit("flex not found (%s)" % args.flex)
    if not os.path.exists(args.lxr):
        sys.exit("lxr not found (%s). Run 'make lxr' in the repo root" % args.lxr)
    modes = [m.strip() for m in args.modes.split(",") if m.strip()]

    cases_dir = os.path.join(args.out, "cases")
    os.makedirs(cases_dir, exist_ok=True)

    cases = []
    for i in range(args.n):
        rng = random.Random(args.seed * 100003 + i)
        nrules = rng.randint(args.min_rules, args.max_rules)
        name = "rand_s%d_%03d" % (args.seed, i)
        path = os.path.join(cases_dir, name + ".l")
        with open(path, "w") as f:
            f.write(gen_lex.generate(rng, nrules))
        cases.append((name, path, nrules, True))
    for path in args.extra:
        name = os.path.splitext(os.path.basename(path))[0]
        nrules = 0
        cases.append((name, os.path.abspath(path), nrules, False))

    rows, skipped = [], []
    for name, path, nrules, is_random in cases:
        lc = os.path.join(cases_dir, name + ".lxr.c")
        L, err = lxr_measure(args.lxr, path, lc)
        if L is None:
            skipped.append((name, err))
            continue
        row = {"case": name, "rules": nrules or "-",
               "lxr_states": L["states"], "lxr_classes": L["classes"],
               "lxr_trans": L["trans"], "lxr_bytes": L["bytes"],
               "lxr_time": round(L["time"], 3)}
        fc_first, ok = None, True
        for mode in modes:
            fc = os.path.join(cases_dir, "%s.flex%s.c" % (name, mode))
            Fm, err = flex_measure(args.flex, mode, path, fc)
            if Fm is None:
                skipped.append((name, err))
                ok = False
                break
            fc_first = fc_first or fc
            row["flex%s_states" % mode] = Fm["states"]
            row["flex%s_classes" % mode] = Fm["classes"]
            row["flex%s_meta" % mode] = Fm["meta"]
            row["flex%s_trans" % mode] = Fm["trans"]
            row["flex%s_bytes" % mode] = Fm["bytes"]
            row["flex%s_time" % mode] = round(Fm["time"], 3)
        if not ok:
            continue
        if is_random and not args.no_check:
            row["tokens"] = check_same(lc, fc_first, cases_dir,
                                       random.Random(zlib.crc32(name.encode())),
                                       args.input_size)
        else:
            row["tokens"] = "-"
        rows.append(row)
        if not args.keep_c:
            for f in glob.glob(os.path.join(cases_dir, name + ".*.c")):
                os.remove(f)
    for f in ("l.out", "s.out", "f.out"):
        f = os.path.join(cases_dir, f)
        if os.path.exists(f):
            os.remove(f)

    # ---------------------------------------------------------------- csv
    keys = ["case", "rules", "lxr_states", "lxr_classes", "lxr_trans",
            "lxr_bytes", "lxr_time"]
    for m in modes:
        keys += ["flex%s_%s" % (m, k) for k in
                 ("states", "classes", "meta", "trans", "bytes", "time")]
    keys.append("tokens")
    with open(os.path.join(args.out, "results.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)

    # ------------------------------------------------------------- report
    lines = []
    head = ["case", "rules", "lxr st", "lxr ec", "lxr chk/nxt", "lxr bytes"]
    for m in modes:
        head += ["flex%s st" % m, "ec", "chk/nxt", "bytes", "lxr/flex bytes"]
    head.append("tokens")
    lines.append("| " + " | ".join(head) + " |")
    lines.append("|" + "---|" * len(head))
    for r in rows:
        cells = [r["case"], str(r["rules"]), str(r["lxr_states"]),
                 str(r["lxr_classes"]), str(r["lxr_trans"]), str(r["lxr_bytes"])]
        for m in modes:
            fb = r["flex%s_bytes" % m]
            ec = r["flex%s_classes" % m]
            if r["flex%s_meta" % m]:
                ec = "%d (meta %d)" % (ec, r["flex%s_meta" % m])
            cells += [str(r["flex%s_states" % m]), str(ec),
                      str(r["flex%s_trans" % m]), str(fb),
                      "%.2f" % (r["lxr_bytes"] / fb) if fb else "-"]
        cells.append(r["tokens"])
        lines.append("| " + " | ".join(cells) + " |")

    # Only cases where both scanners agree are used for the size summary
    valid = [r for r in rows if r["tokens"] in ("same", "-")]
    summary = ["", "Summary (%d compared, %d used for sizes, %d skipped)"
               % (len(rows), len(valid), len(skipped)), ""]
    for m in modes:
        rb = [r["lxr_bytes"] / r["flex%s_bytes" % m] for r in valid
              if r["flex%s_bytes" % m]]
        rt = [r["lxr_trans"] / r["flex%s_trans" % m] for r in valid
              if r["flex%s_trans" % m]]
        smaller = sum(1 for x in rb if x < 1)
        if rb:
            summary.append(
                "  vs flex %-5s : bytes lxr/flex  geomean %.2f  median %.2f "
                "[min %.2f, max %.2f]   chk/nxt entries geomean %.2f   "
                "lxr smaller in %d/%d" % (m, geomean(rb), statistics.median(rb),
                                         min(rb), max(rb), geomean(rt),
                                         smaller, len(rb)))
    toks = [r["tokens"] for r in rows if r["tokens"] != "-"]
    if toks:
        summary.append("  token streams  : %d same, %d differ/other"
                       % (toks.count("same"), len(toks) - toks.count("same")))
    if skipped:
        tally = {}
        for _, why in skipped:
            tally[why] = tally.get(why, 0) + 1
        summary += ["", "Skipped, by reason:"] + \
            ["  %3d  %s" % (k, why) for why, k in
             sorted(tally.items(), key=lambda x: -x[1])]
        summary += ["", "Skipped cases:"] + \
            ["  %-16s %s" % s for s in skipped]
    summary += ["", "Notes:",
                "  - bytes = sum over tables of (entries x C element size)",
                "  - lxr: check next base accept def class, and meta (only with meta classes)",
                "  - flex: yy_accept yy_ec yy_meta yy_base yy_def yy_nxt yy_chk",
                "  - st = DFA states as reported by each tool (flex -v; lxr stats)",
                "  - ec = equivalence classes (lxr: excluding its 4 special classes)",
                "  - tokens = token streams of both scanners on random input"
                " ('same' means both built the same language)"]

    report = "\n".join(lines + summary) + "\n"
    with open(os.path.join(args.out, "report.md"), "w") as f:
        f.write(report)
    print(report)
    print("written : %s/{report.md,results.csv,cases/}" % args.out)
    bad = [t for t in toks if t != "same"]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
