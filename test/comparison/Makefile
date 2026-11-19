# Table size comparison : flex vs lxr
#
#   make                       # 40 random lexer sets + languages/*/*.lex
#   make N=200 SEED=7          # more / other random sets
#   make MODES=-Ce,-Cem,-Cf    # flex table modes to compare against
#   make NOCHECK=1             # skip the token stream (same language) check
#   make clean
#
# Results : out/report.md, out/results.csv, out/cases/*.l

ROOT    = ../..
LXR     = $(ROOT)/lxr
PYTHON ?= python3
FLEX   ?= flex
N      ?= 40
SEED   ?= 1
MODES  ?= -Ce,-Cem
MINR   ?= 5
MAXR   ?= 40

ARGS = --lxr $(LXR) --flex $(FLEX) --n $(N) --seed $(SEED) \
       --modes=$(MODES) --min-rules $(MINR) --max-rules $(MAXR) --out out
ifdef NOCHECK
  ARGS += --no-check
endif

.PHONY: all compare lxr clean

all: compare

# always delegate to the top level Makefile, so lxr is rebuilt if stale
lxr:
	$(MAKE) -C $(ROOT) lxr

compare: lxr
	$(PYTHON) compare.py $(ARGS)

clean:
	rm -rf out __pycache__
