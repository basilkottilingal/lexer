#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "allocator.h"
#include "compression.h"
#include "regex.h"

/* ...................................................................
.. ...................................................................
.. ........  Algorithms related to table compression .................
.. ...................................................................
.. .................................................................*/
#define PARENT_THRESHOLD     0.7
#define CHILD_THRESHOLD      0.9
#define SMALL_THRESHOLD      5
#define EMPTY                -1
#define DEADSTATE            0
#define MAXDEPTH             1

/*
.. A row is a cache corresponding to a state 's', which stores a 
.. cache of (c, δ (s,c)) where the transition δ (s,c) is not a DEAD
.. state.
..    cache(s) := { (c, δ (s,c)) | δ (s,c) ≠ DEAD }
*/

/*
.. Variables used for local use only
*/
typedef struct Delta {
  int c, delta;
} Delta;

static int * check;                                  /* check array */
static int * next;                                    /* next array */
static int * base;                                    /* base array */
static int * def;
static int nclass;                          /* number of eq classes */
static int nstates;                             /* number of states */
static int limit;                 /* allocated size of check & next */

/*
.. Meta equivalence classes (optional, "%option meta-ecs" or "lxr -m").
.. As in flex, a few "template" rows are added after the real states
.. [1, m]. A template t has a single destination tdest[t] for a set of
.. classes tset[t] (ex : all the identifier characters -> IDENT). Real
.. states use a template as their def[] fallback, so they store only
.. the transitions which differ from it. Meta classes refine the
.. equivalence classes by the membership in every tset[t], so the
.. template rows are stored with one entry per meta class :
..   s >= lxr_meta_state (a template)  =>  c <- meta [c]
*/
static int use_meta = 0;
static int ntmpl = 0;                         /* number of templates */
static int tbase = 0;          /* state id of the first template (m+1) */
static int nmeta = 0;                    /* number of meta classes */
static int * tdest = NULL;               /* destination of template t */
static unsigned char * tset = NULL;    /* tset [t*nclass + c] : c in t */
static int * meta = NULL;                         /* class -> meta class */

#define TMPL_MIN_CLASSES     4   /* min |tset| of a template           */
#define TMPL_MIN_STATES      2   /* min states in a group (same dest.) */

void rows_meta ( int on ) { use_meta = on; }
int  rows_meta_state ( ) { return ntmpl ? tbase : 0; }
int  rows_ntemplates ( ) { return ntmpl; }
int  rows_nmeta ( ) { return nmeta; }

static int maxdepth = MAXDEPTH;      /* MAXDEPTH + 1, with templates */

/*
.. Transition δ (s,c) as the generated scanner sees it : look in the
.. row of s, then follow def[] at most 'hops' times. A template's row
.. is indexed by the meta class. Returns EMPTY for DEAD.
*/
static inline
int lookup ( int s, int c, int hops ) {
  for (;;) {
    int k = (ntmpl && s >= tbase) ? meta [c] : c;
    if (check [base [s] + k] == s)
      return next [base [s] + k];
    if (hops-- <= 0 || (s = def [s]) == EMPTY)
      return EMPTY;
  }
}

/*
.. For a candidate state 'ps' (a state already added to check[], or a
.. template) as def[] of row r, the residual is the set of transitions
.. (c, δ (s,c)) which differ from what the scanner finds through 'ps'
.. (including DEAD transitions, stored as (c, EMPTY)).
*/
static inline
int row_candidate ( Row * r, int ps, Delta residual [] ) {
  int n = 0, i = 0;
  Delta * a = (Delta *) r->stack;
  for (int c = 0; c < nclass; ++c) {
    int sv = (i < r->n && a[i].c == c) ? a[i++].delta : EMPTY;
    if (sv != lookup (ps, c, maxdepth - 1))
      residual [n++] = (Delta) {c, sv};
  }
  return n;                                    /* size of residual. */
}

/*
.. Look for a slot to insert a cache of (c, δ (s,c))
*/
static
int find_slot ( Delta residual [], int nr ) {
  for (int offset=0; offset<=limit - nclass; ++offset) {
    int n = nr;
    while (n--)
      if ( check [offset + residual [n].c] != EMPTY )
        break; 
    if (n >= 0) continue;
    return offset;
  }
  return EMPTY;
}

/*
.. Insert a row to check[] ( and corresponndingly to next []),
.. given a parent candidate 'ps' (which may be empty)
*/
static
int row_insert ( Row * r, int ps, Delta residual [] ) {
  Delta * res = (ps != EMPTY) ? residual : (Delta *) r->stack;
  int s = r->s, nr = (ps != EMPTY) ?
    row_candidate (r, ps, residual) : r->n;
  if (!nr)
    return (base [s] = 0);
  int offset = find_slot (res, nr);
  if ( offset == EMPTY ) return RGXERR;
  int n = nr;
  while (n--) {
    check [offset + res [n].c] = s;
    next [offset + res [n].c] = res [n].delta;
  }
  return (base[s] = offset);
}

/*
.. Count number of similar entries among two rows.
*/
static inline
int row_similarity ( Row * r, Row * c ) {
  int n = 0;
  Delta * a = (Delta *) r->stack, 
    * b = (Delta *) c->stack;
  int i = r->n-1, j = c->n-1;
  while ( i>=0 && j>=0 ) {
    if (  a[i].c == b[j].c ) {
      if ( a[i].delta == b[j].delta ) n++;
      i--, j--;
    }
    else if ( a[i].c > b[j].c ) i--;
    else j--;
  }
  return n;
}

/*
.. The transition mapping δ (q, c) is a matrix of size m⋅n where
.. m = |P(Q)|, and n = |P(Σ)| are the size of minimized dfa set and
.. equivalence class. Expecting demanding scenarios, like a C lexer
.. where m ~ 512 and n ~ 64 (just rounded off. Exact numbers depends
.. on which ANSI C version you are using, etc ), you might need a 2D
.. table size of few MBs and traversing through such a large table
.. for every byte input is a bad idea. In scenarios like a C lexer,
.. where this table is very sparse ( i.e for a vary large number of
.. states q, δ (q, c) gives a "dead" state ),
.. you have a scope of compacting this 2D table into 3 linear
.. tables "check[]", "next[]" and "base[]" as mentioned in many
.. literature and also in codes like flex.
..
.. Along with equivalence class table "class[]" and token info table
.. "accept[]" which stores the most preferred token number for an
.. accepting state, and 0 (DEAD) for a non-accpeting state.
.. |check| = |next| = k, where you minimize k.
.. |accept| = |base| = m
..
.. Transition looks like
..   s <- next [base [s] + class [c]], if check[s] == s
..
.. Size of check[], k, is guaranteed to be in the range [n, m⋅n] but
.. finding the minimal solution is an NP-hard problem and thus rely
.. on greedy heauristic algorithm.
*/

static int compare ( const void * a, const void * b ) {
  #define CMP(p,q) cmp = ((int) (p > q) - (int) (p < q));            \
    if (cmp) return cmp
  Row * r = *((Row **)a), * s = *((Row **)b);
  int cmp;                  /* sort by                              */
  CMP (r->n, s->n);         /* number of transitions (decreasing)   */
  CMP (s->hash, r->hash);   /* Compare signature                    */
  CMP (s->s, r->s);         /* state id (lowest preferred)          */
  return 0;
  #undef CMP
}

/*
.. check[], next[] and base[] are written as "short" arrays in the
.. generated lexer, so the table size (and so any offset in base[])
.. has to fit in a short.
*/
#define TABLE_LIMIT          SHRT_MAX

static int resize (int k0) {
  int sold = limit * sizeof (int), s = sold + k0 * sizeof(int);
  if (limit + k0 > TABLE_LIMIT) return RGXOOM;
  check = reallocate (check, sold, s);
  next = reallocate (next, sold, s);
  memset (& check [limit], EMPTY, s - sold);
  limit += k0;
  return 0;
}


#if 0
void row_print ( Row ** rows) {
  int i = 0; Row * r; while ( (r=rows[i++]) ) {
    printf ("\ns%2d n%2d hash%2u token%2d {",
      r->s, r->n, r->hash, r->token);
    for (int j=0; j<r->n; ++j)
      printf ("%s%3d[%3d] ",
        j%16 ? "" :"\n\t",
        r->stack[2*j], r->stack[2*j+1]);
    printf ("}");
  }
}
#endif

/*
.. Create the templates (and the meta classes) from the rows. For each
.. state, its dominant destination d is the one reached by most of the
.. classes. States with the same dominant destination form a group,
.. and the template of the group has the classes that go to d in the
.. majority of the group. A template is kept if it saves more entries
.. than it costs.
*/
static int dom_cmp ( const void * a, const void * b ) {
  const int * p = a, * q = b;
  return (p[0] > q[0]) - (p[0] < q[0]);
}

/*
.. Meta classes : refine the classes by the membership of each class
.. in each template set.
*/
static void meta_create ( ) {
  int n = nclass;
  if (!meta) meta = allocate (n * sizeof (int));
  memset (meta, 0, n * sizeof (int));
  nmeta = ntmpl ? 1 : 0;
  int * map = allocate (2 * n * sizeof (int));
  for (int t = 0; t < ntmpl; ++t) {
    for (int i = 0; i < 2 * nmeta; ++i) map [i] = EMPTY;
    int nm = 0;
    for (int c = 0; c < n; ++c) {
      int key = 2 * meta [c] + tset [t * n + c];
      if (map [key] == EMPTY) map [key] = nm++;
      meta [c] = map [key];
    }
    nmeta = nm;
  }
  deallocate (map, 2 * n * sizeof (int));
}

static void templates_create ( Row ** rows, int m ) {
  int n = nclass;
  int * dom = allocate (2 * m * sizeof (int)),        /* (d, row) */
      * cnt = allocate ((m + 2) * sizeof (int)),
      * ccnt = allocate (n * sizeof (int));
  int ng = 0, cap = 0;

  ntmpl = 0, nmeta = 0, tbase = m + 1;

  for (int k = 0; k < m; ++k) {
    Row * r = rows [k];
    Delta * a = (Delta *) r->stack;
    int best = EMPTY, max = 0;
    for (int i = 0; i < r->n; ++i)
      if (++cnt [a[i].delta] > max) {
        max = cnt [a[i].delta]; best = a[i].delta;
      }
    for (int i = 0; i < r->n; ++i)
      cnt [a[i].delta] = 0;
    if (max >= TMPL_MIN_CLASSES) {
      dom [2*ng] = best; dom [2*ng+1] = k; ng++;
    }
  }
  qsort (dom, ng, 2 * sizeof (int), dom_cmp);

  for (int g = 0; g < ng; ) {
    int d = dom [2*g], h = g;
    while (h < ng && dom [2*h] == d) h++;
    int size = h - g;
    if (size >= TMPL_MIN_STATES) {
      memset (ccnt, 0, n * sizeof (int));
      for (int k = g; k < h; ++k) {
        Row * r = rows [dom [2*k+1]];
        Delta * a = (Delta *) r->stack;
        for (int i = 0; i < r->n; ++i)
          if (a[i].delta == d) ccnt [a[i].c]++;
      }
      int nset = 0;
      for (int c = 0; c < n; ++c)
        nset += (ccnt [c] = (2 * ccnt [c] > size));
      if (nset >= TMPL_MIN_CLASSES) {
        /* gain : entries saved by the states of the group */
        int gain = 0;
        for (int k = g; k < h; ++k) {
          Row * r = rows [dom [2*k+1]];
          Delta * a = (Delta *) r->stack;
          int res = 0, i = 0;
          for (int c = 0; c < n; ++c) {
            int sv = (i < r->n && a[i].c == c) ? a[i++].delta : EMPTY;
            res += (sv != (ccnt [c] ? d : EMPTY));
          }
          if (res < r->n) gain += r->n - res;
        }
        if (gain > nset) {
          if (ntmpl == cap) {
            int ncap = cap ? 2 * cap : 16;
            tdest = reallocate (tdest, cap * sizeof (int),
              ncap * sizeof (int));
            tset = reallocate (tset, cap * n, ncap * n);
            cap = ncap;
          }
          tdest [ntmpl] = d;
          for (int c = 0; c < n; ++c)
            tset [ntmpl * n + c] = (unsigned char) ccnt [c];
          ntmpl++;
        }
      }
    }
    g = h;
  }

  meta_create ();

  deallocate (ccnt, n * sizeof (int));
  deallocate (cnt, (m + 2) * sizeof (int));
  deallocate (dom, 2 * m * sizeof (int));
}

/*
.. Insert the template rows (one entry per meta class) to check[] and
.. next[]. Returns the largest base used, or RGXERR.
*/
static int resize (int k0);

static int templates_insert ( int * def, int * accept, int k0 ) {
  Delta row [256];
  int offset = 0;
  for (int t = 0; t < ntmpl; ++t) {
    int nr = 0, s = tbase + t;
    unsigned char seen [256] = {0};
    for (int c = 0; c < nclass; ++c)
      if (tset [t * nclass + c] && !seen [meta [c]]) {
        seen [meta [c]] = 1;
        row [nr++] = (Delta) { meta [c], tdest [t] };
      }
    int loc;
    while ( (loc = find_slot (row, nr)) == EMPTY )
      if (resize (k0)) return RGXOOM;
    for (int i = 0; i < nr; ++i) {
      check [loc + row [i].c] = s;
      next [loc + row [i].c] = row [i].delta;
    }
    base [s] = loc;  def [s] = EMPTY;  accept [s] = 0;
    if (loc > offset) offset = loc;
  }
  return offset;
}

/*
.. It is a heauristic approach. We have some rows of some density and
.. size and we have to place one row after the other, such that
.. (a) when we insert a row, 'r', see if there is a suitable candidate
.. row, 'p' which is already inserted. The candidate is chosen such a
.. way that the number of entries common between the rows 'c' and 'p'
.. are maximum. So we add only the entries which are not found in the
.. candidate row. NOTE : (We have to consider REJECT transitions also,
.. when considering the number of match/mismatch).
.. Considering caches R(r), R(p), the cost of inserting r and then p,
.. can be assumed as |R(r)| + |R(r)\R(p)| + |R(p)\R(r)|.
.. (The third term on sum is for REJECT). So you can see, it can be
.. ideal to insert smaller set first.
.. (b) It's also intuitive to insert larger caches and later insert
.. smaller caches in the empty slots.
.. So following algorithm uses both technique. First insert few larger
.. rows. Then other rows are inserted in the order of increasing cache
.. size, and each time looking for a suitable parent row.
*/
/*
.. One compression of the rows (in the sorted order) into check[] and
.. next[], with the current templates (ntmpl may be 0). "rows" is
.. modified (NULL terminated at a restart index). Returns the size of
.. check[] (and next[]), or RGXOOM / RGXERR.
*/
static int compress_rows ( Row ** rows, int n, int * accept ) {

  Delta residual [256];
  int k0 = 4 * n;   /* let's start with k=4n & reallocate if needed */
  k0 = 1 << (64 - __builtin_clzll ((unsigned long long)(k0 - 1)) );

  if (check) {                         /* from a previous compression */
    deallocate (check, limit * sizeof (int));
    deallocate (next, limit * sizeof (int));
  }
  limit = k0;
  check = allocate (limit* sizeof (int));
  next = allocate (limit * sizeof(int));
  memset ( check, EMPTY, limit * sizeof (int) );

  int offset = 0, startindex = 0;

  /*
  .. Templates (if any) are inserted first. Template states are
  .. numbered [m+1, m+ntmpl].
  */
  if ( ntmpl && (offset = templates_insert (def, accept, k0)) < 0 )
    return RGXOOM;

  for (int niter = 0; niter < 2; ++niter ) {

    if (startindex) {
      rows [startindex] = NULL;
      startindex = 0;
    }

    Row * r;
    for (int irow=0; (r = rows [irow]) != NULL; ++irow ) {

      if (offset + 2*n > limit)   /* resize next[], check[] if reqd */
        if (resize (k0)) {
          return RGXOOM;
        }

      int s = r->s, jrow = irow - 1, nrows = 0, min = INT_MAX,
        best = EMPTY, queue [2];
      /*
      .. We look among the rows that are already added, to see if 
      .. it is a good candidate to be taken as the def[this state].
      .. The best candidate is chosen by minimum of |residual|,
      .. where residual is the set of transitions (c, delta) which are
      .. not found in the cache of candidate.
      */
      while (jrow >= startindex && nrows++ < 16) {
        queue [0] = rows [jrow]->s, queue [1] = def [queue [0]];
        for (int iq =0; iq < 2 && queue [iq] != EMPTY; ++iq) {
          int nres = row_candidate ( r, queue [iq], residual );
          if (nres < min) { min = nres; best = queue [iq]; }
        }
        jrow --;
      }

      /*
      .. A template is a candidate for def[], if its destination is
      .. one of this row's destinations.
      */
      if (ntmpl) {
        Delta * a = (Delta *) r->stack;
        for (int t = 0; t < ntmpl; ++t) {
          int d = tdest [t], found = 0;
          for (int i = 0; i < r->n && !found; ++i)
            found = (a[i].delta == d);
          if (!found) continue;
          int nres = row_candidate ( r, tbase + t, residual );
          if (nres < min) { min = nres; best = tbase + t; }
        }
      }

      if ( best != EMPTY &&
        ( rows [irow+1] != NULL && r->n > SMALL_THRESHOLD ) &&
        ( min > (int) ((1.0 - PARENT_THRESHOLD) * r->n) ) &&
        ( row_similarity (r, rows[irow+1]) > 
          (int) (CHILD_THRESHOLD* r->n)) ) 
      {
        /*
        .. After looking for possible parent candidates, we look if
        .. there is a row further down (not yet added to check[])
        .. which is very similar to this row. So we can skip splitting
        .. this row. We use this, so there is no "def" chaining for
        .. for those future rows.
        */
        best = EMPTY;

        #if 1
        /*
        .. What we do here is we clear the check and next table, thus
        .. removing the rows in [0, strtindex) out of the table &
        .. insert all the rows in [0, strtindex) in the next iteration
        .. (niter == 1). Why we do is that because, shorter rows like
        .. that in [0, strtindex) are easier to place in the gaps.
        */
        if (!niter){
          /* We will insert the rows in [0, strindex) in next itern*/
          startindex = irow;
          memset (check, EMPTY, (offset + n) * sizeof (int)); 
          memset (next, 0, (offset + n) * sizeof (int));
          if (ntmpl) {          /* templates are inserted first again */
            int to = templates_insert (def, accept, k0);
            if (to < 0) return RGXOOM;
          }
        }
        #endif
      }

      /*
      .. We set the def [], and accept [] token of this state
      .. check[], base[] and next[] will be set inside "row_insert()".
      */
      def [s] = best;
      accept [s] = r->token;
      int loc = row_insert ( r, best, residual );
      if (loc == RGXERR) {
        error ("table compression : internal error");
        return RGXERR;
      }
      if (loc > offset) offset = loc;
    }

    if (!startindex)
      break;
  }

  return offset + n;
}

/*
.. Templates that no state uses as def[] are removed (and the meta
.. classes are recomputed, so they may get coarser). Returns the
.. number of templates removed. (A template with a single user is
.. kept : with def[] chains, that user may be the def[] of many.)
*/
static int templates_prune ( int m ) {
  int * users = allocate ((ntmpl + 1) * sizeof (int)), k = 0;
  for (int s = 1; s <= m; ++s)
    if (def [s] != EMPTY && def [s] >= tbase)
      users [def [s] - tbase]++;
  for (int t = 0; t < ntmpl; ++t)
    if (users [t]) {
      if (k != t) {
        tdest [k] = tdest [t];
        memcpy (& tset [k * nclass], & tset [t * nclass], nclass);
      }
      k++;
    }
  deallocate (users, (ntmpl + 1) * sizeof (int));
  int removed = ntmpl - k;
  ntmpl = k;
  if (removed) meta_create ();
  return removed;
}

int rows_compression ( Row ** rows, int *** tables, 
  int ** tsize, int m, int n )
{

  nstates = m, nclass = n;

  /*
  .. Sort the rows by increasing number of entries. Group rows with
  .. same signature together. Signature is evaluated from the last and
  .. first entry of cache.
  */
  qsort (rows, m, sizeof (Row*), compare);

  base = tables [0][2];
  def  = tables [0][4];
  int * accept = tables [0][3];

  /*
  .. compress_rows () modifies the list of rows, so each compression
  .. works on a copy.
  */
  Row ** work = allocate ((m + 1) * sizeof (Row *));
  #define COMPRESS(_size_)                                           \
    do {                                                             \
      memcpy (work, rows, m * sizeof (Row *));  work [m] = NULL;     \
      maxdepth = ntmpl ? MAXDEPTH + 1 : MAXDEPTH;                    \
      (_size_) = compress_rows (work, n, accept);                    \
    } while (0)

  /*
  .. Optional : templates & meta classes. base[], def[], accept[] are
  .. extended for the template states [m+1, m+ntmpl]. Templates that
  .. are rarely used are removed, and finally templates are kept only
  .. if the tables (in bytes) are smaller than without templates.
  */
  int size = 0, ntmpl0;
  ntmpl = 0;
  if (use_meta) {
    templates_create (rows, m);
    if (ntmpl) {
      size_t o = (m+1) * sizeof (int), z = (m+1+ntmpl) * sizeof (int);
      base   = tables [0][2] = reallocate (base, o, z);
      accept = tables [0][3] = reallocate (accept, o, z);
      def    = tables [0][4] = reallocate (def, o, z);
    }
    int iter = 0;
    while (ntmpl) {
      COMPRESS (size);
      if (size < 0 || ++iter == 4 || !templates_prune (m))
        break;
    }
    if (!ntmpl) size = 0;            /* all pruned : compress again */
    if (ntmpl && size > 0) {
      int bytes = 4 * size + 6 * (m + 1 + ntmpl) + n, size0;
      ntmpl0 = ntmpl;  ntmpl = 0;
      COMPRESS (size0);
      if (size0 < 0 || bytes < 4 * size0 + 6 * (m + 1)) {
        ntmpl = ntmpl0;
        COMPRESS (size);
      }
      else
        size = size0;
    }
    else if (ntmpl) {          /* failed with templates : try without */
      ntmpl = 0;
      COMPRESS (size);
    }
  }
  if (!ntmpl && !size)
    COMPRESS (size);
  #undef COMPRESS

  deallocate (work, (m + 1) * sizeof (Row *));
  if (size < 0) {
    if (size == RGXOOM)
      error ("Table compression: Out of table size limit %d",
        TABLE_LIMIT);
    else
      error ("table compression : internal error");
    return size;
  }

  if (ntmpl) {
    tsize [0][2] = tsize [0][3] = tsize [0][4] = m + 1 + ntmpl;
    tables [0][5] = meta;  tsize [0][5] = nclass;
  }

  deallocate (rows, (m+1)*sizeof (Row*));

  tsize [0][0] = tsize [0][1] = size;
  tables [0][0] = check;  tables[0][1] = next;

  for (int i=0; i<tsize [0][0]; ++i) {
    if (check [i] == EMPTY) 
      check [i] = DEADSTATE;
    if (next [i] == EMPTY) 
      next [i] = DEADSTATE;
  }
  for (int i=0; i<tsize [0][4]; ++i)
    if (def [i] == EMPTY)
      def [i] = DEADSTATE;
  /*
  .. Now we have
  .. State 0 : for  reject/dead state.
  .. State 1 : start state
  .. State 2 : start state (if BOL)
  */

  return 0;
}

#undef EMPTY
#undef DEADSTATE
