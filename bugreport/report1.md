# Lexer generator test report

Tested `src/lexer` (binary, sha1 `5b0d5053920e596d94b1394211c02b6f67d82259`, 102,136 bytes)
against flex 2.6.4 on 2026-11-01.

**Summary:** the unmodified `src/lexer.l` now builds, and the generated scanner matched
flex token for token on all 1,779 test inputs. The test also found two memory bugs in the
generated scanner and a generator crash. Table size is unchanged: 7,211 bytes against
flex's 5,148.

## Status of previously reported problems

| # | Problem | Status |
|---|---|---|
| 1 | Macro directly followed by a newline | **Fixed**: same tables as with macros inlined |
| 2 | Trailing space in a macro | **Fixed**: same tables with a trailing space or CRLF |
| 3 | Bare actions | **Changed**: now a warning, 85 on `src/lexer.l`; those actions still come through intact |
| 4 | `{` in a character class | **Fixed** |
| 5 | Crash on exit | **Fixed**: `src/lexer.l` exits 0 every time, with macros kept |
| 6 | `lxr_meta` unused | **Still there**: all zeros and never read |
| 7 | `check/next` too large | **Unchanged** |
| 8 | Weak default-row compression | **Unchanged** |
| 9 | Garbage in error messages | **Still there**: for example on `(a` and `a\|` |
| 10 | Partial output on failure | **Still there**: 801-byte file left at exit 255 |
| 11 | Message issues | **Partly fixed**: see below |

For item 11, "characterbetween" and the false parenthesis warning on `PFX` are gone.
These remain:

- The "paranthesis" typo.
- The missing newline after that warning (`macro IDfailed creating lexer code`).
- `-d` missing from the usage text.

## Table comparison

| | Flex `-Cem` (default) | Flex `-Ce` | `src/lexer` |
|---|---:|---:|---:|
| Bytes | 5,148 | 6,110 | 7,211 |
| `check/next` entries | 570 | 827 | 1,102 |

| | Flex | `src/lexer` |
|---|---:|---:|
| States | 419 | 411 |
| Equivalence classes | 65 | 65 |

Flex byte counts are measured from the compiled object. `src/lexer` byte counts come from
the declared table types (`short` and `unsigned char`).

`-Ce` is the closest flex mode, because `src/lexer` has no working meta classes.

## Scanner test against flex

Both scanners were built from the same rules (`src/lexer.l` with actions in braces) and
each printed every token.

- **Inputs:** 1,779 files, 13 MB and 315,319 tokens in total. They include real `gcc -E`
  output, edge cases (NUL and high bytes, unterminated strings, tokens up to 200,000 bytes,
  tokens across the 16 KB buffer boundary) and seeded random files.
- **Compared for each token:** kind, length, text, `strlen(yytext)`, line, file and trivia.
- **Result:** no differences. The 128-entry state stack caused no problems.
- **Sensitivity check:** three deliberately altered grammars were all caught.
- **Not tested:** `lxr_read_bytes`, `lxr_input`, `lxr_unput`, and linking the scanner into
  `cwrap`.

## New bugs

### 1. Memory overflow on the first token of every input

`lxr_yytext_dummy[] = "\n"` is 2 bytes, and `yytext` starts at index 1. That makes
`yytext[acc_len]` and the `'\0'` write after it land one byte past the end.

### 2. Memory overflow at buffer refill

This hit 109 inputs, all at least 16 KB, including all six real `gcc -E` files. `bytes` is
allocated at `size + 2`, but `yytext = bytes + 1` can reach `yytext[size + 1]`.

### 3. Generator segfault on an unclosed `[`

`[a`, `[`, `[a-` and `[^` all crash in `rpn_nfa` when used in a rule. In a macro or at end
of file, `[a` gives a normal error instead. This had not been tested before, so it may not
be new.

## Suggested fix for the overflows

Neither overflow changed any tokens in the normal build, but both are real out-of-bounds
accesses that also write, so they could break on another compiler or allocator.

These two changes need to go into the generator's output template:

1. Declare `lxr_yytext_dummy[3]`.
2. Change `size + 2` to `size + 3` in the `bytes` allocations (`lxr_alloc` twice,
   `lxr_realloc` once).

With both applied to a scratch copy of the generated scanner, ASan and UBSan were clean on
all 1,779 inputs, and tokens still matched flex.
