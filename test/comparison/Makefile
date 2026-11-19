# Table size comparison : flex vs lxr
#
#   make                       # 40 random lexer sets + languages/*/*.lex
#   make N=200 SEED=7          # more / other random sets
#   make MODES=-Ce,-Cem,-Cf    # flex table modes to compare against
#   make NOCHECK=1             # skip the token stream (same language) check
#   make META=1                # lxr with meta classes (lxr -m)
#   make plot                  # README plots from out/results.csv
#   make bench                 # scanning speed, see bench.py
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
ifdef META
  ARGS += --lxr-flags=-m
  BENCHARGS += --meta
endif

.PHONY: all compare plot bench lxr clean

all: compare

# scanning speed : lxr vs flex (out/bench/bench.md)
#   make bench SIZE=64 RUNS=7 IO=both
#   make bench META=1          (also lxr -m)
SIZE ?= 32
RUNS ?= 5
IO   ?= mem
bench: lxr
	$(PYTHON) bench.py --lxr $(LXR) --flex $(FLEX) --size $(SIZE) \
	  --runs $(RUNS) --io $(IO) --seed $(SEED) $(BENCHARGS)

# light & dark SVG plots for README.md (docs/img/flex-comparison-*.svg)
plot:
	$(PYTHON) plot.py --csv out/results.csv --out $(ROOT)/docs/img

# always delegate to the top level Makefile, so lxr is rebuilt if stale
lxr:
	$(MAKE) -C $(ROOT) lxr

compare: lxr
	$(PYTHON) compare.py $(ARGS)

clean:
	rm -rf out __pycache__
