# Lexer memory-safety report

Tested `src/lexer` (binary, sha1 `bb3035e47412b386b0380b2baf71d2983a8b9c6f`) on 2026-11-01.
Focus: memory overflows only, no flex comparison.

**Headline:** memory bugs remain in two places.

1. The **generated scanner** still overflows at runtime. Of the two fixes suggested last
   time, `size + 2 → size + 3` was applied but `lxr_yytext_dummy[3]` was not, so the
   first-token global overflow is still there, and the harder inputs exposed further heap
   overflows that neither fix covers.
2. The **generator itself** (the `lexer` binary) crashes with memory-safety faults on many
   grammars, including a **stack buffer overflow**. These are new findings; earlier runs
   didn't probe the generator this hard.

## Method

- **Sanitizers:** scanners compiled with ASan + UBSan (`-fsanitize=address,undefined`,
  recover, use-after-scope), run over a large matrix of grammars x `LXR_BUFF_SIZE`
  (1,2,3,4,7,16,64,default) x input x driver mode. Small buffer sizes force the refill path
  on every input. ~31k sanitizer finding lines collected.
- **Driver modes:** `lxr_source` (file), stdin, `lxr_read_bytes` (bol 0/1), plus randomized
  interleavings of `lxr_lex` / `lxr_input` / `lxr_unput` / `lxr_token`, buffer reuse, and
  source switching. The driver touches `yytext[-1 .. yyleng]` and `strlen` after every token.
- **Generator fuzzing:** 442 oversized/malformed structured `.l` files and 6,000 mutated
  `.l` files, each run under glibc and under a custom **guard-page allocator** (every
  allocation flanked by `PROT_NONE` pages, left- and right-aligned) to catch heap overreads
  that glibc hides. Crashes bucketed by signal and stack via gdb.
- **Inputs:** NUL and high bytes, unterminated strings/chars, tokens up to 131072 bytes,
  tokens straddling the 16 KB buffer boundary, real `gcc -E` output.

## Generated scanner (runtime)

Both are real out-of-bounds accesses that read **and** write.

### 1. Global-buffer-overflow on `lxr_yytext_dummy` — NOT FIXED

`lxr_yytext_dummy[] = "\n"` is still 2 bytes with `yytext = &dummy[1]`. On the first token
of every input, `yytext[acc_len]` reads and `yytext[acc_len]='\0'` writes one byte past the
array. Dominant finding in the matrix. Fix: declare `lxr_yytext_dummy[3]`.

### 2. Heap-buffer-overflow on buffer refill — PARTIALLY ADDRESSED, STILL FAILS

`size + 3` is now allocated, but overflows remain on the refill path
(`lxr_buffer_update`, generated ~line 436/698) for tokens that span a buffer boundary,
especially unterminated strings with EOL-anchored backup. Observed both a right-side
overflow and a **2-bytes-to-the-left underflow** of the realloc'd region
(`lxr_lex` reading `yytext[acc_len]` with a negative `acc_len`). Hit at every small buffer
size and on real `gcc -E` files at the default size.

### Also seen at runtime

- `lxr_buffer_update () : internal check failed` aborts.
- `lxr_realloc failed` aborts (allocation sizing on large tokens).
- A few heap-use-after-free and leaks in source-switch / reuse sequences.

A scratch patch (`dummy[3]` + tracking `bf->size`) reduced but did **not** eliminate the
runtime errors — the refill/backup overflow needs a real fix, not just a bigger allocation.

## Generator (the `lexer` binary itself)

Found by fuzzing; each is a memory-safety crash in the tool, not a clean error exit.

| Crash site | Trigger | Signal |
|---|---|---|
| **`nfa_reset`** (stack buffer overflow) | large counted repetition, e.g. `a{4096}` | SIGSEGV / SIGBUS, ASLR-dependent |
| `rpn_nfa` | character class past a size threshold; reversed range `[z-a]`; full range `[\x00-\xff]`; high-byte class | SIGSEGV |
| `rgx_dfa_tree` / `hopcroft` | repeated stars, e.g. `a****…` (as few as 2) | SIGSEGV |
| `state` / `dfa_root` | nested quantifiers `((a{100}){100}){100}` | SIGSEGV, and SIGFPE (divide-by-zero) |
| `block_available` (`src/allocator.c:47`) | grammars with roughly ≥ 350 rules | SIGABRT: `assert(s <= PAGE_SIZE)` |

### Most serious: stack overflow in `nfa_reset`

`nfa_reset` has a fixed 0xc60-byte stack frame holding three ~1024-entry `int` arrays
(`-0xc10`, `-0x810`, `-0x410(%rbp)`) written as `arr[counter]` with an unbounded counter
driven by the repetition count. `a{4096}` indexes well past 1024. Because it's a stack
write past fixed buffers, outcome is ASLR-dependent: of 40 runs of `a{4096}`, 8 crashed
(SIGSEGV/SIGBUS) and the rest returned 255 after very likely corrupting the stack without
faulting. The stack canary (`__stack_chk_fail` is present) did not trip in the crashing
runs. Attacker/user-controlled repetition count writing past a stack array is the highest
severity item here.

### Notes

- Reversed ranges and full-byte ranges should be a clean diagnostic, not a crash.
- The `block_available` assertion is a hard cap: the page/bump allocator cannot serve an
  allocation larger than one page, so large rule sets abort.
- The garbage-bytes error message seen earlier reproduces: its stderr content varied with
  allocator fill pattern on identical input, i.e. an uninitialized/stale read in the error
  path. Cosmetic.

## Suggested fixes

Generated scanner:

1. `lxr_yytext_dummy[3]`.
2. Re-derive the refill/backup logic so `yytext[acc_len]` can never go left of the buffer
   or past the filled region; `size + 3` alone is insufficient.

Generator:

3. Bound the `nfa_reset` state arrays (heap-allocate and grow, or reject on overflow)
   before indexing.
4. Guard `rpn_nfa` / NFA construction against large and malformed classes, reversed and
   full ranges, and repeated/nested quantifiers; emit a diagnostic instead of faulting.
5. Let the allocator serve >1 page, or cap rule count with a diagnostic.
