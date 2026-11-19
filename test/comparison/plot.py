#!/usr/bin/env python3
"""
Plot lxr vs flex table sizes from out/results.csv (written by compare.py).

Writes two static SVGs (light and dark), for use in README.md with
<picture> / prefers-color-scheme:

  python3 plot.py [--csv out/results.csv] [--out ../../docs/img]

x : flex table size in bytes, y : lxr table size in bytes (log-log).
One dot per lexer and flex mode. Below the diagonal, lxr is smaller.
Only cases where both scanners produce the same tokens are plotted.
"""

import argparse
import csv
import math
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))

THEMES = {
    "light": dict(surface="#fcfcfb", text="#0b0b0b", text2="#52514e",
                  muted="#8a8984", grid="#e6e5e1", axis="#b9b8b2",
                  s1="#2a78d6", s2="#eb6834"),
    "dark": dict(surface="#1a1a19", text="#f0efec", text2="#c3c2b7",
                 muted="#8f8e88", grid="#2e2e2c", axis="#55544f",
                 s1="#3987e5", s2="#d95926"),
}
FONT = ("-apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, "
        "Arial, sans-serif")


def nice_ticks(lo, hi):
    """1-2-5 ticks covering [lo, hi] (log scale)."""
    ticks = []
    e = math.floor(math.log10(lo))
    while 10 ** e <= hi * 10:
        for m in (1, 2, 5):
            v = m * 10 ** e
            if lo <= v <= hi:
                ticks.append(v)
        e += 1
    return ticks


def fmt(v):
    return "%gk" % (v / 1000) if v >= 1000 else "%g" % v


def flex_version():
    try:
        out = subprocess.run(["flex", "--version"], capture_output=True,
                             text=True).stdout.split()
        return out[-1] if out else "?"
    except OSError:
        return "?"


def svg(rows, theme, version, title_counts):
    t = THEMES[theme]
    W, H = 720, 500
    L, R, T, B = 72, 28, 92, 58              # plot margins
    pw, ph = W - L - R, H - T - B

    pts = []
    for r in rows:
        y = float(r["lxr_bytes"])
        for key, series in (("flex-Ce_bytes", "s1"), ("flex-Cem_bytes", "s2")):
            if r.get(key):
                pts.append((float(r[key]), y, series, r["case"]))
    vals = [p[0] for p in pts] + [p[1] for p in pts]
    lo = 10 ** (math.floor(math.log10(min(vals)) * 4) / 4)
    hi = 10 ** (math.ceil(math.log10(max(vals)) * 4) / 4)

    def X(v):
        return L + pw * (math.log10(v) - math.log10(lo)) / \
            (math.log10(hi) - math.log10(lo))

    def Y(v):
        return T + ph - ph * (math.log10(v) - math.log10(lo)) / \
            (math.log10(hi) - math.log10(lo))

    o = []
    o.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
             'viewBox="0 0 %d %d" font-family="%s" role="img" '
             'aria-labelledby="t d">' % (W, H, W, H, FONT))
    o.append('<title id="t">Table size: lxr vs flex %s</title>' % version)
    o.append('<desc id="d">Log-log scatter of generated table sizes in bytes. '
             'x is flex, y is lxr, one dot per lexer for flex -Ce and -Cem. '
             'Points below the diagonal mean lxr tables are smaller.</desc>')
    o.append('<rect width="%d" height="%d" fill="%s"/>' % (W, H, t["surface"]))

    # title, subtitle
    o.append('<text x="%d" y="30" font-size="17" font-weight="600" fill="%s">'
             'Generated table size: lxr vs flex %s</text>' % (L - 48, t["text"], version))
    o.append('<text x="%d" y="52" font-size="13" fill="%s">%s. '
             'Below the line, lxr tables are smaller.</text>'
             % (L - 48, t["text2"], title_counts))

    # legend (above plot, left)
    lx, ly = L - 48, 76
    o.append('<circle cx="%d" cy="%d" r="5" fill="%s"/>' % (lx + 5, ly - 4, t["s1"]))
    o.append('<text x="%d" y="%d" font-size="12.5" fill="%s">vs flex -Ce '
             '(no meta classes, like lxr)</text>' % (lx + 16, ly, t["text2"]))
    lx2 = lx + 250
    o.append('<rect x="%d" y="%d" width="9" height="9" rx="1.5" fill="%s"/>'
             % (lx2, ly - 8.5, t["s2"]))
    o.append('<text x="%d" y="%d" font-size="12.5" fill="%s">vs flex -Cem '
             '(flex default)</text>' % (lx2 + 16, ly, t["text2"]))

    # grid + ticks
    ticks = nice_ticks(lo, hi)
    for v in ticks:
        x, y = round(X(v)) + 0.5, round(Y(v)) + 0.5     # crisp 1px lines
        o.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="%s"/>'
                 % (x, T, x, T + ph, t["grid"]))
        o.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="%s"/>'
                 % (L, y, L + pw, y, t["grid"]))
        o.append('<text x="%.1f" y="%d" font-size="11.5" fill="%s" '
                 'text-anchor="middle">%s</text>' % (x, T + ph + 18, t["muted"], fmt(v)))
        o.append('<text x="%d" y="%.1f" font-size="11.5" fill="%s" '
                 'text-anchor="end" dominant-baseline="middle">%s</text>'
                 % (L - 8, y, t["muted"], fmt(v)))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s"/>'
             % (L, T + ph, L + pw, T + ph, t["axis"]))
    o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s"/>'
             % (L, T, L, T + ph, t["axis"]))

    # axis titles
    o.append('<text x="%.1f" y="%d" font-size="12.5" fill="%s" text-anchor="middle">'
             'flex table size (bytes, log scale)</text>' % (L + pw / 2, H - 14, t["text2"]))
    o.append('<text transform="translate(18 %.1f) rotate(-90)" font-size="12.5" '
             'fill="%s" text-anchor="middle">lxr table size (bytes, log scale)</text>'
             % (T + ph / 2, t["text2"]))

    # diagonal y = x  (equal size)
    o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" '
             'stroke-width="1.5" stroke-dasharray="5 4"/>'
             % (X(lo), Y(lo), X(hi), Y(hi), t["muted"]))
    # label along the diagonal, near its upper end, just above the line
    # (horizontal : some SVG renderers ignore rotate() on text)
    v = 10 ** (math.log10(lo) + 0.93 * (math.log10(hi) - math.log10(lo)))
    o.append('<text x="%.1f" y="%.1f" font-size="11.5" fill="%s" '
             'text-anchor="end">same size</text>' % (X(v) - 8, Y(v) - 2, t["muted"]))
    o.append('<text x="%.1f" y="%.1f" font-size="12" fill="%s" text-anchor="end">'
             'lxr smaller &#8595;</text>' % (L + pw - 12, T + ph - 14, t["text2"]))

    # points: -Cem (squares) first, -Ce (circles) on top. 2px surface ring.
    for series in ("s2", "s1"):
        for fx, ly_, s, _ in pts:
            if s != series:
                continue
            x, y = X(fx), Y(ly_)
            if s == "s1":
                o.append('<circle cx="%.1f" cy="%.1f" r="4.5" fill="%s" '
                         'stroke="%s" stroke-width="2"/>' % (x, y, t[s], t["surface"]))
            else:
                o.append('<rect x="%.1f" y="%.1f" width="9" height="9" rx="1.5" '
                         'fill="%s" stroke="%s" stroke-width="2"/>'
                         % (x - 4.5, y - 4.5, t[s], t["surface"]))

    # direct labels for the hand written grammars (their -Ce point)
    for fx, ly_, s, name in pts:
        if s == "s1" and not name.startswith("rand_"):
            o.append('<text x="%.1f" y="%.1f" font-size="12" font-weight="600" '
                     'fill="%s">%s</text>' % (X(fx) + 9, Y(ly_) + 4, t["text"], name))

    o.append('</svg>')
    return "\n".join(o) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=os.path.join(HERE, "out", "results.csv"))
    ap.add_argument("--out", default=os.path.join(HERE, "..", "..", "docs", "img"))
    ap.add_argument("--name", default="flex-comparison")
    args = ap.parse_args()

    rows = [r for r in csv.DictReader(open(args.csv))
            if r["tokens"] in ("same", "-")]
    nrand = sum(1 for r in rows if r["case"].startswith("rand_"))
    names = [r["case"] for r in rows if not r["case"].startswith("rand_")]
    counts = "%d random lexers + %s" % (nrand, ", ".join(names))
    version = flex_version()
    os.makedirs(args.out, exist_ok=True)
    for theme in THEMES:
        path = os.path.join(args.out, "%s-%s.svg" % (args.name, theme))
        with open(path, "w") as f:
            f.write(svg(rows, theme, version, counts))
        print("written", path)


if __name__ == "__main__":
    main()
