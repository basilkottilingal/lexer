/*
.. Given an NFA (Q, Σ, δ, q0 , F ) there exists
.. atleast one partition P (Σ), which satisfy the condition ( cond-1 )
.. ∀ p[i] ∈ P and c, c′ ∈ p[i]   ⇒   δ(q, c) = δ(q, c′)  ∀  q ∈ Q
.. Now, the equivalence class is the function
.. C(c) : Σ → {0, 1, ..., |P| − 1} which maps c to p[i]
.. i.e.   C(c) = i ⇐⇒ c ∈ p[i]
..
.. The aim of evaluating equivalence classes is (a) to reduce 
.. the size of transition (nfa->next[]) table from |Σ| to |P|, and 
.. (b) to reduce the computational cost in the algorithms like
.. Hopcroft minimization ( O (m⋅n⋅log(n)) ) where m is the
.. size of equivalence class, and n is the number of dfa states.
.. So our aim is to look for the coarsest partition, or the one
.. that satisfy above condition (cond-1) and minimises |P(Σ)|.
..
.. We initialize ( class_init() )  with
.. P ← {Σ}
.. for each character group S, we refine P ( class_refine() ) as :
..    P ← { Y ∩ S, Y ∖ S ∣ Y ∈ P } ∖ { ∅ }
..
.. In the above algorithm, S, is a character set
.. like [a-z], [^0-9], etc.
.. Neglect intersection set or difference set, if empty.
..
.. Creating equivalence class for negated character class [^..], works
.. same way for non-negated class []. It is because the refining algo,
.. class_refine (), has similar effect. If ¬S := Σ∖S, is the 
.. complement of S, then : Y ∩ ¬S = Y ∖ S, and Y ∖ ¬S = Y ∩ S. 
  
.. We return nclass = nclass + 2 to accomodate the BOL and EOL.
*/

#define ALPH 256

#include <string.h>

#include "class.h"

/*
.. head and next are used to maintain a linked list of characters in
.. an equivalence class. END is used to mark the end of the list.
*/
static int next [ALPH], head [ALPH + 1]; /* head [nclass] is written
                                      .. even when nclass == ALPH */

/*
.. class is the mapping C(c) from [0,256) -> [0, nclass) where nclass
.. is the number of equivalence classes. i.e nclass = max(C) + 1 = |P|
*/
static int class [ALPH], nclass = 0;

void class_get ( int ** c, int * n ) {
  /*
  .. We reserve four equivalence classes at the end: BOL, EOL, EOF,
  .. and EOB (end of buffer)
  */
  *c = class; *n = nclass + BCLASSES;
}

#define END  -1
void class_init () {

  /*
  .. Initialize with just one equivalence classes :
  ..  P ← { Σ },  and C(c) = 0 ∀ c in Σ
  .. and refine later as required by character class, character groups
  .. or stand alone characters in the regex patterns.
  */

  nclass = 1;
  head [0] = 0;
  for(int i=0; i<ALPH-1; ++i) {
    class [i] = 0;
    next  [i] = i+1;
  }
  next [ALPH-1] = END;

}

/*
.. Refine to accomodate a new equivalence class
*/
void class_refine ( int * list, int nlist ) {
  #define INSERT(Q,q)          *Q = q; Q = & next [q]

  int S [ALPH], * Y1, * Y2, c, n, ec;
  memset (S, 0, sizeof (S));
  for (int i=0; i<nlist; ++i)
    S [list[i]] = 1;

  for (int i=0; i<nlist; ++i) {

    c = list [i];

    if ( S[c] == 0 )                /* Alreay placed to a new class */
      continue ;

    ec = class [c];
    Y1 = & head [ec] ;
    Y2 = & head [nclass] ;

    c = *Y1;
    while ( c != END ) {
      n = next [c];
      if ( S [c] ) {
        INSERT (Y1, c);                               /* Y1 = Y ∩ S */
      }
      else {
        INSERT (Y2, c);                               /* Y2 = Y ∖ S */
        S [c] = 0;
        class [c] = nclass;
      }
      c = n;
    }
    *Y1 = *Y2 = END;                     /* Now Y is replaced by Y1 */

    if ( (c = head [nclass]) != END ) {              /* Y ∖ S  ≠ ∅  */
      nclass++;                        /* Add Y2 to P, if non-empty */
    }
  }

}

/*
.. Refine to accomodate a new quivalence class that
.. contains only one character.  S := {c}
*/
void class_char ( int c ) {
  int ec = class [c], * Y1 = & head [ec];
  if ( *Y1 == c && next [c] == END )
    return;                                   /* c is already alone */

  int y, * Y2 = & head [nclass];
  while ( (y = *Y1) != END ) {
    if (y == c) {
      INSERT (Y2, c);
      *Y1 = next [c];
      *Y2 = END;
      class [c] = nclass++;
      return;
    }
    Y1 = & next [y];
  }
}

#undef INSERT
#undef END


