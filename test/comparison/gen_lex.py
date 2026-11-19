"""
Random lexer-set (.l file) generator for the flex vs lxr comparison.

Generated grammars only use the subset of the lex format that both
flex and lxr accept, and that has the SAME meaning in both:

  - literals, escaped specials, quoted strings
  - character classes [..], negated classes [^..], ranges
  - operators  *  +  ?  |  ( )  {m}  {m,n}  {m,}
  - '^' (BOL anchor) at the beginning of a pattern
  - macros (definitions section), always wrapped in ( ) or [ ], so that
    lxr (plain text substitution) and flex (implicit parenthesis) agree

Not used, because lxr doesn't implement them or treats them differently:

  - trailing context / look ahead  'a/b'
  - <<EOF>> rules, start conditions <S>
  - '$' (flex implements it as trailing context a/\\n, lxr as EOL/EOF)
  - \\d \\w \\s ..., grouped rules (PAT1 | newline PAT2 {..})

Every pattern consumes at least one character (lxr rejects zero-length
tokens). The last two rules are catch-alls ('.' and '\\n') so that both
scanners never "jam", and token streams can be compared.
"""

import random

# Characters used for literals (outside classes). Specials get escaped.
LIT_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGXYZ0123456789_=<>!&%:;,+*.#@~"
# '%' is special at the beginning of a rule in flex, and so is '<' (a
# start condition <S>). Both are escaped always.
SPECIALS = set('|*?+[](){}^$-.\\"/%<')
# Characters used inside character classes. Avoid ] \ ^ - which need
# special handling (and are handled differently at class boundaries).
CLASS_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGXYZ0123456789_=<>!&%:;,+*.#@~"
RANGES = ["a-z", "A-Z", "0-9", "a-f", "A-F", "g-p", "q-z", "0-7", "!-/", ":-@"]
WORDS = ["if", "else", "while", "for", "do", "int", "char", "return",
         "true", "false", "null", "def", "let", "var", "fn", "end", "begin",
         "case", "goto", "auto", "enum", "union", "void", "and", "or", "not"]


def lit_char(rng):
    c = rng.choice(LIT_CHARS)
    return "\\" + c if c in SPECIALS else c


def char_class(rng):
    parts = []
    for _ in range(rng.randint(1, 3)):
        if rng.random() < 0.5:
            parts.append(rng.choice(RANGES))
        else:
            parts.append("".join(rng.sample(CLASS_CHARS, rng.randint(1, 4))))
    if rng.random() < 0.15:
        # negated classes exclude '\n' too : otherwise the catch-all
        # '\n' rule is often unreachable, and lxr rejects the lexer
        return "[^" + "".join(parts) + "\\n]"
    return "[" + "".join(parts) + "]"


def quoted(rng):
    s = "".join(rng.choice("abcxyz+*.()|?=<>") for _ in range(rng.randint(1, 5)))
    return '"' + s + '"'


def atom(rng, depth, macros, quotes=True):
    r = rng.random()
    if macros and r < 0.12:
        return "{" + rng.choice(macros) + "}"
    if r < 0.35:
        return char_class(rng)
    if r < 0.55:
        return "".join(lit_char(rng) for _ in range(rng.randint(1, 3)))
    if r < 0.62:
        # lxr doesn't support quoted strings inside macro definitions
        return quoted(rng) if quotes else lit_char(rng)
    if depth < 1 and r < 0.80:
        alts = [mandatory(rng, depth + 1, macros, quotes)
                for _ in range(rng.randint(2, 3))]
        return "(" + "|".join(alts) + ")"
    return char_class(rng)


def quantify(rng, a, optional_ok):
    """Apply a random quantifier. If not optional_ok, the result must
    still consume at least one character."""
    r = rng.random()
    if r < 0.45:
        return a
    if r < 0.65:
        return a + "+"
    # Note : x{m,n} is expanded to m+n copies of x by both tools. Keep
    # the counts small, or the NFA quickly outgrows lxr's internal limits
    if r < 0.72:
        m = rng.randint(1, 2)
        return a + "{%d,%d}" % (m, m + rng.randint(0, 2))
    if r < 0.77:
        return a + "{%d}" % rng.randint(1, 3)
    if r < 0.8:
        return a + "{%d,}" % rng.randint(1, 2)
    if optional_ok:
        return a + rng.choice("*?")
    return a + "+"


def mandatory(rng, depth, macros, quotes=True):
    """A regex that never matches the empty string."""
    seq = [quantify(rng, atom(rng, depth, macros, quotes), False)]
    for _ in range(rng.randint(0, 2)):
        seq.append(quantify(rng, atom(rng, depth, macros, quotes), True))
    return "".join(seq)


def expanded_len(p, bodies):
    """Length of pattern p as lxr sees it : macros substituted, and
    specials inside quotes escaped (one extra byte each)."""
    for name, body in bodies.items():
        p = p.replace("{" + name + "}", body)
    extra = sum(seg.count(c) for seg in p.split('"')[1::2] for c in SPECIALS)
    return len(p) + extra


def pattern(rng, macros):
    """Return (priority, pattern). Fixed strings (keywords/operators)
    get priority 0, so that they are placed before the generic patterns
    that would otherwise shadow them (lxr rejects unreachable rules)."""
    r = rng.random()
    if r < 0.25:
        prio, p = 0, rng.choice(WORDS)                        # keyword
    elif r < 0.35:
        prio, p = 0, "".join(lit_char(rng) for _ in range(rng.randint(1, 3)))
    else:
        prio, p = 1, mandatory(rng, 0, macros)
    if rng.random() < 0.08:
        p = "^" + p
    return prio, p


def generate(rng, nrules, nmacros=None):
    """Return the text of a random .l file with nrules (+2 catch-all)
    rules. Token numbers are 1..nrules+2."""
    if nmacros is None:
        nmacros = rng.randint(0, 4)
    macros, bodies = [], {}
    lines = ["%option noyywrap", "%{", "#include <stdio.h>",
             "#include <string.h>", "%}"]
    while len(macros) < nmacros:
        name = "M%d" % len(macros)
        body = char_class(rng) if rng.random() < 0.6 else \
            "(" + mandatory(rng, 1, [], quotes=False) + ")"
        if len(body) > 60:
            continue
        lines.append("%-8s %s" % (name, body))
        macros.append(name)
        bodies[name] = body
    lines.append("%%")
    seen, rules = set(), []
    while len(rules) < nrules:
        prio, p = pattern(rng, macros)
        # lxr's regex buffer is RGXSIZE (256) bytes. Stay well below it.
        if p in seen or expanded_len(p, bodies) > 150:
            continue
        seen.add(p)
        rules.append((prio, p))
    rules.sort(key=lambda x: x[0])                       # stable sort
    for k, (_, p) in enumerate(rules, 1):
        lines.append("%-40s { return %d; }" % (p, k))
    lines.append("%-40s { return %d; }" % (".", nrules + 1))
    lines.append("%-40s { return %d; }" % ("\\n", nrules + 2))
    lines.append("%%")
    lines += [
        "#ifdef FLEX_SCANNER",
        "  #define LEXFN yylex",
        "#else",
        "  #define LEXFN lxr_lex",
        "#endif",
        "int main () {",
        "  int t;",
        "  while ( (t = LEXFN ()) )",
        "    printf (\"%d %d\\n\", t, (int) strlen (yytext));",
        "  return 0;",
        "}",
    ]
    return "\n".join(lines) + "\n"


def random_input(rng, size):
    """Random text built from the characters used in the grammars."""
    alphabet = LIT_CHARS + "()|?[]{}\"'\\-^$/ \t" + "\n" * 4
    words = WORDS
    out = []
    while sum(len(x) for x in out) < size:
        if rng.random() < 0.3:
            out.append(rng.choice(words))
        else:
            out.append(rng.choice(alphabet))
    return "".join(out).encode()


if __name__ == "__main__":
    import sys
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    sys.stdout.write(generate(random.Random(seed), n))
