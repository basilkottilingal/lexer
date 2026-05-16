#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "nfa.h"
#include "class.h"
#include "regex.h"
#include "allocator.h"
#include "stack.h"
#include "bits.h"

/*
.. Functions and objects required for creating a
.. DFA ( Deterministic Finite Automaton )
*/

typedef struct DState {
  Stack * list;
  struct DState ** next;
  struct DState * hchain;
  Stack * bits;
  int i, flag;                            /* preferred token number */
  uint32_t hash;
} Dstate;

static DState ** states = NULL;               /* list of dfa states */
static int       nstates = 0;              /* Number of Dfa states. */
static DState ** htable  = NULL;                /* Uses a hashtable */
static int       hsize;                /* fixme : use prime numbers */
static int       stacksize;    /* bit stack size rounded to 8 bytes */
static int     * class = NULL;
static int       nclass = 0;

static DState * state ( Stack * list, Stack * bits, int * exists) {
  states_bstack (list, bits);
  uint32_t hash = stack_hash ((uint32_t *) bits->stack, bits->max);
  DState ** ptr = & htable [hash % hsize], * d;
  *exists = 1;
  while ( (d = *ptr) != NULL ) {          /* resolve hash collision */
    if (d->hash == hash && !stack_cmp (bits, d->bits))
      return d;
    ptr = & d->hchain;
  }
  *exists = 0;
  d = allocate ( sizeof (DState) );
  *d = (DState) {
    .next = allocate ( nclass * sizeof (DState *)),
    .hash = hash,
    .flag = RGXMATCH (list),
    .list = stack_copy ( list ),
    .bits = stack_copy ( bits )
  };
  return (*ptr = d);
}

static DState * dfa_root ( State * nfa, int nnfa ) {

  Stack * list = stack_new (0);
  Stack * bits = stack_new (stacksize);
  #define RTN(r) stack_free (list); stack_free (bits); return r

  State ** buff[RGXSIZE];
  int status = states_at_start ( nfa, list, buff );

  if (status < 0) { RTN (NULL); }

  nstates = 0;                   /* fixme : Cleaned previous nodes? */
  int n = 6, exists, primes[] = /* prime > 2^N for N in { 6, 7, .. }*/
    { 67, 131, 257, 521, 1031, 2053, 4099, 8209, 16411 };
  while ((1<<n) < nnfa) n++;
  stacksize = BITBYTES (nnfa);        /* rounded as 64 bits multiple*/
  hsize = primes[ n - 6 ];
  while (hsize * sizeof (void *) > PAGE_SIZE)       /* memory limit */
    hsize = primes[ --n - 6 ];
  htable = allocate ( hsize * sizeof (State *) );
  DState * root = state (list, bits, &exists);
  RTN (root);

  #undef RTN
}

/* ...................................................................
.. ...................................................................
.. ........  Algorithms related to DFA minimisation  .................
.. ...................................................................
.. .................................................................*/

/*
.. Traverse through the DFA tree starting from "root".
.. The full dfa set will be stored in *states = Q. And also in the
.. hashtable "htable", where the key is the bit set corresponding to
.. NFA Cache.
*/
static int
rgx_dfa_tree ( DState * root, Stack ** Qptr ) {
  #define RTN(r)  stack_free (list); stack_free (bits);              \
    if (r<0) stack_free (Q); else *Qptr = Q;                         \
    return r

  DState * dfa;
  int c, exists = 0;
  struct tree {
    DState * d; State ** s; int n;
  } stack [ RGXSIZE ];
  State ** buff[RGXSIZE], *s;
  Stack * list = stack_new (0),
    * bits = stack_new (stacksize),
    * Q = stack_new(0);                   /* Q   : set of DFA nodes */

  stack[0] = (struct tree) {
    root, (State **) root->list->stack,
    root->list->len / sizeof (void *)
  };
  int state_id = 0, n = 1;                     /* Depth of the tree */
  while (n) {

    /*
    .. Go down the tree if 'next' dfa node is a newly created node
    */
    while ((s = stack[n-1].n ? stack[n-1].s[--stack[n-1].n] : NULL)) {
                                   /* iterate each nfa of dfa cache */
      dfa = stack[n-1].d;
      c = s->id;
      if (c >= 256 || dfa->next[c])
        continue;

      if (states_transition (dfa->list, list, buff, c) < 0) {
        RTN (RGXERR);
      }

      dfa->next[c] = state (list, bits, &exists);
      if (!exists) {     /* 'dfa' was recently pushed to hash table */
        dfa = dfa->next[c];
        if (n == RGXSIZE) { RTN (RGXOOM); }
        stack[n++] = (struct tree) {   /* PUSH() & go down the tree */
          dfa, (State **) dfa->list->stack,
          dfa->list->len / sizeof (void *)
        };
      }
    }

    do {
      dfa = stack[--n].d; /* Pop a dfa from stack, add the dfa to Q */
      dfa->i = state_id++;
      stack_push (Q, dfa);                     /* Set of all states */
    } while ( n && !stack[n-1].n );
  }

  states = (DState **) Q->stack;
  RTN (0);

  #undef RTN
}

  #define POP(B)          ( B->len == 0 ? NULL :                     \
    ((uint64_t **) B->stack)                                         \
    [ (B->len -= sizeof (void *))/sizeof(void *) ] )
  #define PUSH(S,B)       stack_push (S, B)
  #define FREE(B,s)       BITCLEAR (B,s); PUSH (pool, B)
  #define BSTACK(B)       uint64_t * B =                             \
    pool->len ? POP(pool) : allocate (qsize)
  #define STACK(S)        Stack * S = stack_new (0)
  #define COPY(S)                                                    \
    memcpy ( allocate (qsize), S, qsize )

static int dfa_minimal ( Stack * Q, Stack * P, DState ** dfa ) {

  /*
  .. Converting Partition P(Q) (which is a bit set representation) to
  .. a new DFA table Q' and store it in the global cache states [].
  .. Also, identifying accepting states of Q'. Lowest token number
  .. will be assigned to q->flag for all q in Q'.
  .. ( Assumes : lower the itoken, more the precedence )
  */

  uint64_t i64, ** bitstack = (uint64_t **) P->stack, * Y;
  int nq = Q->len/sizeof (void *), np = P->len/sizeof (void *),
    qsize = BITBYTES (nq);
  DState * d, ** next, * child,                        /* iterators */
    ** q = (DState **) Q->stack,             /* original dfa states */
    ** p =  (hsize < np) ?                        /* new dfa states */
    allocate ( np * sizeof (DState *) ) :
    memset ( htable, 0, np * sizeof (DState *) );   /* reuse memory */
  Stack * cache = stack_new (0);       /* stacking pointers of q[i] */

  /*
  .. We reserved states [0] for root DFA node, and states [1] node for
  .. root DFA that satisfy BOL condition (in case string start with a
  .. BOL flag). Inorder to do that we do a mapping 
  .. { root DFA :0, root BOL DFA : 1, others : [2,np-1] }
  */
  int * map = allocate (np * sizeof (int)), mapindex = 2, bol = 
    q [nq-1]->next [BOL_CLASS] ? q [nq-1]->next [BOL_CLASS]->i : -1;
  if (bol == -1) {
    error ("warning : Non-anchored pattern should start with ^?.\n"
      ".. Cannot find single BOL edge for root DFA");
    return RGXERR;
  }
  memset (map, -1, np * sizeof (int));

  /*
  .. Allocating Memory for q[i] and assigning values.
  */  
  for (int j=0; j<np; ++j) {                  /* each j in [0, |P|) */

    d = allocate (sizeof (DState));                    /* p[j] in P */
    int tkn = 0;

    stack_reset (cache);
    Y = bitstack[j] ;
    for (int k = 0; k < qsize >> 3; ++k) {         /* decoding bits */
      int base = k << 6, bit = 0;
      i64 = Y[k];
      while (i64) {
        bit = __builtin_ctzll(i64);       /* position of lowest bit */

        child = q [bit|base];
        if (child->i == nq-1) {   /* root dfa is the TOP of Q stack */
          *dfa = d;                  /* root of the new DFA tree Q' */
          map [j] = 0;
        }
        else if (child->i == bol)
          map [j] = 1;
        else if (map [j] == -1) {
          if (mapindex == np) {
            error ( "Wrong dfa mapping to states [] cache.\n"
              ".. Internal error");
            return RGXERR;
          }
          map [j] = mapindex++;
        }

        child->i = map [j];    /* from now 'i' map each q[i] to p[j]*/
        stack_push (cache, child);                  /* Cache of q[i]*/
        stack_free (child->bits); child->bits = NULL;
        if (RGXMATCH (child))
          tkn = RGXMATCH (child);

        i64 &= (i64 - 1);                   /* clear lowest set bit */
      }
    }

    /*
    .. Where to place this dfa in states [] array.
    .. states [0] is reserved for root/start DFA.
    .. states [1] is reserved for root/start DFA that satisfy BOL
    */
    *d = (DState) {
      .list = stack_copy (cache),
      .i    = map [j],
      .flag = tkn,
      .next = allocate (nclass * sizeof (DState *))
    };
    p [ map[j] ] = d;
  }
  stack_free (cache);

  /*
  .. Internal check to see if root DFA and root BOL DFA are
  .. located?
  */
  if ( p[0] == NULL || p[1] == NULL || mapindex < np ) {
    error ("DFA Internal error : failed mapping.");
    return RGXERR;
  }

  /*
  .. (b) Creating the transition for each p[i]
  */
  int c, n, m;
  for (int j=0; j<np; ++j) {
    d = p[j];
    cache = d->list;
    DState ** s = (DState **) cache->stack;
    n = cache->len / sizeof (void *);
    next = d->next;

    while (n--) {
      State ** nfa = (State **) s[n]->list->stack;
      m = s[n]->list->len / sizeof (void *);
      while (m--)
        if ( (c = nfa[m]->id) < 256 && c >= 0 && !next[c] ) {
          if (s[n]->next[c] == NULL) return RGXERR;
          next[c] = p[ s[n]->next[c]->i ];
        }
      stack_free (s[n]->list); s[n]->list = NULL;
    }
    stack_free (cache); d->list = NULL;
  }

  /*
  .. Set global variables.
  .. (a) states : final cache of minimzed DFA
  .. (b) nstates : |states|
  */
  states = p;
  nstates = np;
  return 1;
}

#if 0
void print_partition (Stack * P, int qsize) {
  int _np = P->len / sizeof (void *);
  uint64_t ** _bits = (uint64_t **) P->stack;
  int _nbits;
  printf ("\n  |P| = %d; \n  P = { ", _np);
  for(int i=0; i<_np;++i) {
    BITCOUNT(_bits[i], _nbits, qsize);
    printf ("%d {", _nbits);
    for (int j=0; j<qsize>>3; ++j) {
      uint64_t b = _bits[i][j];
      while (b) {
        printf ("%d,", __builtin_ctzll (b) + 64 * j);
        b &= b-1;
      }
    }
    printf ("} ");
  }
  printf ("}");
}
#endif

/*
.. Given a "root" NFA, it returns minimized DFA (*dfa)
*/

static int
hopcroft ( State * nfa, DState ** dfa, int nnfa, int ntokens ) {

  #define RTN(r) stack_free (Q); stack_free (P); return (r);

  /*
  .. Given an NFA "nfa", it will first create the root dfa, and from
  .. which the entire list of dfa states are created and connected via
  .. transitions using rgx_dfa_tree (). The list of dfa states are
  .. stored in the stack 'Q' (same as global variable 'states').
  */
  Stack * Q;
  DState * root = dfa_root (nfa, nnfa); if (!nfa) return RGXERR;
  if ( rgx_dfa_tree (root, &Q) < 0 || !Q )return RGXERR;
  int nq = Q->len / sizeof (void *), qsize = BITBYTES(nq);
  DState ** q = (DState **) Q->stack, * next;
  #if 0
  printf ("\n |Q| %d ", nq); fflush (stdout);
  #endif

  if (ntokens > nq) {
    error ("dfa : Bad Lexer Design. "
      "Some tokens are never reachable?!");
    return RGXERR;
  }

  /*
  .. Maintain a pool of bitsets. Create the partition set P(Q) stored
  .. in stack 'P'. 'W' is a temporary stack used in Hopcroft alogirthm
  */
  int rounded =                           /* rounded, 2^N >= 1 + nq */
    1 << (64 - __builtin_clzll ((unsigned long long) nq) );
  Stack * pool = stack_new (rounded * sizeof (void *)),
    * P = stack_new (rounded * sizeof (void *)),
    * W = stack_new (rounded * sizeof (void *));

  /*
  .. Lets's initialize P with a very coarse partition of Q. Most
  .. commonly adopted one is P <- { F, Q\F }, where F is the subset of
  .. Q which are accepting states. But in case of lexers, where you
  .. need to distinguish which token is accepted, you cannot put all
  .. accepting states into one large set F. Instead you start with
  .. P <- { F_0, F_1, ... F_(t-1), Q \ (F_0 ∪ F_1 ∪.. ∪ F_(t-1)) },
  .. where F_k is the set of accepting DFAs for token number = k, and
  .. t is the total number of tokens.
  .. Since we design such a way that token k has precedence over all
  .. tokens k' with k' > k, then if there is a string which matches
  .. both patterns k and k' > k, we put the accepting dfa in F_k.
  .. This initial partition P, makes sure that you can identify which
  .. token matched the string. (In case of match collision, it gives
  .. the lowest token number that satisfied the input string)
  */

  {
    uint64_t ** y = (uint64_t **) P->stack, * f;
    for (int i=0; i<nq; ++i) {
      int token = RGXMATCH (q[i]);
      if (! y [token]) {
        BSTACK (bset);
        y [token] = bset;
      }
      f = y [token];
      BITINSERT (f, i);
    }

    uint64_t ** w = (uint64_t **) W->stack;
    for (int t=1; t<=ntokens; ++t) {
      if (! y[t]) {
        error ("dfa : Bad Lexer Design. "
          "Some tokens are never reachable?!");
        return RGXERR;
      }
      w[t-1] = COPY (y[t]); // FCOPY [t * 64];
    }

    P->len = (ntokens+1) * sizeof (void *);
    W->len = ntokens * sizeof (void *);
    if (!y[0]) {
      y [0] = y [ntokens]; y [ntokens] = NULL;
      P->len -= sizeof (void *);
    }

  }

  #if 0
  print_partition (P, qsize);
  #endif

  /*
  ..  function hopcroft(DFA):
  ..    P := { F , Q \ F }
  ..    W := { F }
  ..
  ..    while W not empty:
  ..      A := remove some element from W
  ..      for each input symbol c:
  ..        X := { q in Q | δ(q,c) in A }
  ..        for each block Y in P that intersects X:
  ..          Y1 := Y ∩ X
  ..          Y2 := Y \ X
  ..          replace Y in P with Y1, Y2
  ..          if Y in W:
  ..            replace Y with Y1, Y2 in W
  ..          else:
  ..            add smaller of (Y1,Y2) to W
  ..    return P
  */
  int count1 = 0, count2 = 0;
  BSTACK (X); BSTACK (Y1); BSTACK (Y2);
  while (W->len) {                                 /* while |W| > 0 */
    uint64_t * A = POP (W);                        /* A <- POP (W)  */
    int c = nclass;
    while ( c-- ) {                          /* each c in [0, P(Σ)) */
      BITCLEAR (X, qsize);
      int k = 0;
      for (int i=0; i<nq; ++i) {
        if( (next = q[i]->next[c]) && BITLOOKUP (A, next->i) ) {
          k++; BITINSERT (X, i);   /* X <- { q in Q | δ(q,c) in A } */
        }
      }
      if (!k) continue;
      int nP = P->len/sizeof(void *);
      for (int i=0; i<nP; ++i) {                   /* each Y in P   */
        uint64_t ** y = (uint64_t **) P->stack,
          ** w = (uint64_t **) W->stack;
        uint64_t * Y = y [i];
        BITAND ( Y, X, Y1, k, qsize );             /* Y1 <- Y ∩ X   */
        if (!k) continue;                          /* Y ∩ X = ∅     */
        BITMINUS ( Y, X, Y2, qsize );              /* Y2 <- Y \ X   */
        BITCOUNT ( Y1, count1, qsize );            /* |Y1|          */
        BITCOUNT ( Y2, count2, qsize );            /* |Y2|          */
        if ( !count2 ) continue;                   /* Y \ X = ∅     */
        y [i] = COPY (Y1);
        PUSH ( P, COPY (Y2) );
        k = -1;
        int issame;
        for ( int l=0; l<W->len/sizeof(void *); ++l) {
          BITCMP ( Y, w[l], issame, qsize );
          if (issame) {                            /* if (Y ∈ W)    */
            FREE ( w[l], qsize );
            w[l] = COPY (Y1) ;
            PUSH (W, COPY(Y2));
            k = l; break;
          }
        }
        if (k<0)                                   /* if (Y ∉ W)    */
          PUSH ( W, count1 <= count2 ? COPY(Y1) : COPY(Y2) );
        FREE ( Y, qsize );
      }
    }
  }

  /*
  .. Partition of the set Q is now stored in P.
  .. (i)   P = {p_0, p_1, .. } with p_i ⊆ Q, and p_i ≠ ∅
  .. (ii)  p_i ∩ p_j = ∅ iff i ≠ j
  .. (iii) collectively exhaustive, i.e,  union of p_i is Q
  */
  stack_free(W); stack_free (pool);
  int np = P->len/sizeof (void *);
  #if 0
  print_partition (P, qsize);
  printf ("|Q'| %d", np);
  #endif
  dfa [0] = q[nq-1];
  int rval = ( nq >= np ) ?
    dfa_minimal( Q, P, dfa) :
    RGXERR;                              /* |P(Q)| should be <= |Q| */
  RTN (rval);

  #undef RTN

}

  #undef BSTACK
  #undef POP
  #undef FREE
  #undef PUSH
  #undef STACK
  #undef COPY

int rgx_list_dfa ( char ** rgx, int nr, DState ** dfa ) {
  int n, nt = 0;
  State * nfa = allocate ( sizeof (State) ),
    ** out = allocate ( (nr+1) * sizeof (State *) );
  nfa_reset ( rgx, nr );
  class_get ( &class, &nclass );
  for (int i=0; i<nr; ++i) {
    n = rgx_nfa (rgx[i], &out[i], 1);
    if ( n < 0 ) {
      error ("rgx list nfa : cannot create nfa for rgx \"%s\"", rgx);
      return RGXERR;
    }
    nt += n;
  }
  *nfa = (State) {
    .id  = NFAEPS, .ist = nt++, .out = out
  };

  return hopcroft (nfa, dfa, nt, 1);
}

static int num_actions = 0;
int rgx_lexer_dfa ( char ** rgx, int nr, DState ** dfa ) {
  int n, nt = 0;
  State * nfa = allocate ( sizeof (State) ),
    ** out = allocate ( (nr+2) * sizeof (State *) );
  nfa_reset ( rgx, nr );
  class_get ( &class, &nclass );
  for (int i=0; i<nr; ++i) {
    /*
    .. Note that the token number itoken = 0, is reserved for error
    */
    n = rgx_nfa (rgx[i], &out[i], i+1);
    if ( n < 0 ) {
      error ("rgx list nfa : cannot create nfa for rgx \"%s\"", rgx);
      return RGXERR;
    }
    nt += n;
  }
  num_actions = nr;
  {
    /*
    .. handling EOF :
    .. δ (0, class [EOF]) = EOF_STATE;
    .. accept (EOF_STATE) = nr + 1; 
    */
    n = rgx_nfa ("x", &out[nr], nr+1);         /* a dummy regex "x" */
    if (n < 0) {
      error ("rgx list nfa : cannot create EOF transition");
      return RGXERR;
    }
    State * nfa = out [nr]; assert (nfa->id == NFAEPS);
    nfa = nfa->out[0];      assert (nfa->id == BOL_CLASS);
    nfa = nfa->out[0];      assert (nfa->id == class ['x']);
    nfa->id = EOF_CLASS;         /* replace class['x'] by EOF class */
    nt += n;
  }
  *nfa = (State) {
    .id  = NFAEPS,
    .ist = nt++,
    .out = out
  };
  return hopcroft (nfa, dfa, nt, nr + 1);
}

int rgx_dfa ( char * rgx, DState ** dfa ) {
  return rgx_list_dfa (&rgx, 1, dfa);
}

int rgx_dfa_match ( DState * dfa, const char * txt ) {
  const char *start = txt, *end = NULL;
  DState * d = dfa;
  int c;
  if (RGXMATCH (d))  end = txt;
  while ( (c = 0xFF & *txt++) ) {
    if ( (d = d->next[class[c]]) == NULL )
      break;
    if (RGXMATCH (d)) end = txt;
  }
  /* return val = number of chars that match rgx + 1 */
  return end ? (int) (end - start + 1) : 0;
}

/* ...................................................................
.. ...................................................................
.. ........  Algorithms related to table compression .................
.. .............. coninued in src/compression.c ......................
.. .................................................................*/
#include "compression.h"

/*
.. Create a stack of rows ( each row corresponds to a state ). We add
.. 1 to the state number, so that we can use state id = 0 for REJECT.
.. We also add EOB_CLASS transition to all states.
*/
static Row ** rows_create () {
  #define EMPTY        -1
  #define HASH         2166136261u
  #define NEWSTATE(_s) (_s+1)

  int m = nstates + 1, n = nclass;
  Row ** rows = allocate ( (m+2) * sizeof (Row *) );
  for (int k=0; k<m;) {
    int nr = m - k;
    if (nr > PAGE_SIZE / sizeof (Row)) nr = PAGE_SIZE / sizeof (Row);
    Row * mem = allocate (nr * sizeof (Row));
    while (nr--) rows [k++] = mem++;
  }

  int stack [512], ntrans, eob_state = nstates;
  DState * eob = allocate (sizeof (DState)),
         * eof = states[0]->next [EOF_CLASS];

  if (!eof || (eof != states[1]->next [EOF_CLASS])) {
    error ("internal error : EOF transition not set properly");
    return NULL;
  }

  *eob = (DState) {
    .i = eob_state,
    .flag = RGXMATCH (eof) + 1,     /* internally used accept state */
    .next = allocate (n*sizeof (DState *))   /* Null, for all class */
  };

  for (int s=0; s<nstates; ++s) {

    DState ** d = states[s]->next;
    if (states [s] != eof)
      /* add EOB transition to each states except EOF states */
      d [EOB_CLASS] = eob;

    ntrans = 0;
    for (int c = 0; c < n; ++c)
      if (d[c]) {
        stack [ntrans++] = c;
        stack [ntrans++] = NEWSTATE (d[c]->i);
      }

    /*
    .. A simple hashing involving first & last entries of the
    .. transition cache
    */
    uint32_t h = HASH;
    if ( ntrans) {
      h = (h ^ (uint32_t) stack [0]) * 16777619u;
      h = (h ^ (uint32_t) stack [1]) * 16777619u;
      h = (h ^ (uint32_t) stack [ntrans-1]) * 16777619u;
      h = (h ^ (uint32_t) stack [ntrans-2]) * 16777619u;
    }

    int * copy = allocate (sizeof (int) * ntrans);
    memcpy ( copy, stack, sizeof (int) * ntrans );
    *(rows[s]) = (Row) {
      .s = NEWSTATE (s), .n = ntrans/2, .hash = h,
      .token = RGXMATCH (states [s]), .stack = copy
    };
  }

  *(rows[eob_state]) = (Row) {               /* row corresponds EOB */
    .s = NEWSTATE (eob_state), .n = 0, .hash = HASH,
    .token = RGXMATCH (eob), .stack = allocate (8)
  };

  return rows;

  #undef EMPTY
  #undef NEWSTATE
  #undef HASH
}

int dfa_eol_used () {
  for (int i=0; i<nstates; ++i)
    if (states [i]->next [EOL_CLASS] != NULL)
      return 1;
  return 0;
}

int dfa_tables (int *** tables, int ** tsize) {

  /*
  .. A lexer cannot allow zero length tokens, because this non
  .. consuming token will run the lexing function infinitely
  */
  if ( RGXMATCH (states[0]) || RGXMATCH (states[1]) ) {
    error ("zero length token not allowed");
    return RGXERR;
  }

  /*
  .. Very special case, highly improbable. In case number of classes
  .. for alphabets + {EOB, EOF, EOL, EOB} exceeds 256.
  */
  if (nclass > 256) {
    error ("highly refined eq class. internal limit");
    return RGXOOM;
  }

  /*
  .. Since we know the state which corresponds to BOL is states [1],
  .. we switch off the transition from 0->1 by BOL_CLASS.
  */
  states [0]->next [ BOL_CLASS ] = NULL;
  int ** t = * tables = allocate ( 7 * sizeof (int *) );
  int * len = * tsize = allocate ( 7 * sizeof (int) );

  int n = nclass, m = nstates + 1;  /* additional '1' for EOB state */
  len [2] = len [3] = len [4] = m + 1;      /* can hold index : [m] */
  len [5] = n;  len [6] = 256;

  int * base = allocate (m * sizeof (int)),
    * accept = allocate (m * sizeof (int)),
    * def = allocate (m * sizeof (int)),
    * meta = allocate (n * sizeof (int));
  /*
  .. Compressed tables, t[0] = check[], t[1] = next;
  .. will be set inside row_compression ().
  */
  t [2] = base;   t [3] = accept;   t [4] = def;
  t [5] = meta;   t [6] = class;

  /*
  .. Will return the compressed linear tables, if it didn't
  .. encounter common errors like very large table requirement
  */
  Row ** rows = rows_create ();
  if (!rows) return RGXERR;
  int status = rows_compression (rows, tables, tsize, m, n );
  if (status < 0) return RGXERR;

  #if 1                        /* fixme : make it compiler optional */
  printf ("\nstats :"
    "\n  no: of states   %3d"
    "\n  no: of tokens   %3d"
    "\n  no: of eq class %3d (excluding EOF, EOL, BOL, EOB)"
    "\n  table sizes     %3d (check[], next[])"
    "\n", nstates, num_actions, nclass - BCLASSES, len[0] ); 
  #endif

  return 0;
}
