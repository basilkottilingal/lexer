/*
.. test case for nfa.
.. $ make test-nfa.s && ./test-nfa
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"
#include "nfa.h"

int main () {
  char * rgx[] = 
  { 
    "aa|b", "a[^c]+", "bc*", "b.a", "(a|b)+0", "bc[a-z]+",
    "(11*0+0)(0+1)*0*1*", "\\\\u[0-9a-fA-F]{4}"
  };

  int nrgx = sizeof (rgx) / sizeof (rgx[0]);
  nfa_reset (rgx, nrgx);

  const char txt[] = 
    "aaccbqaabbaba01bcccd1100011010010\\u1a0F01010001001000111101010101001";

  char buff[60];
  State * nfa = NULL;
  for (int i=0; i< sizeof (rgx) / sizeof (rgx[0]); ++i) {
    int status = rgx_nfa (rgx[i], &nfa, 1);
    if (status < RGXEOE) 
      printf ("failed in creating NFAi for rgx %s: %d", rgx[i], status); 

    const char * source = txt;
    do {
      int m = rgx_nfa_match (nfa, source);
      if (m < 0) printf ("failed in match check %d", m); 
      //source += m > 1 ? (m-1) 
      if (m > 0) {
        m -= 1; buff[m] = '\0';
        if(m) memcpy (buff, source, m);
        printf ("\nRegex %s: found in txt \"%s\" (\"%s\")", rgx[i], buff, source);
      }
    } while (*++source);
  }

  /* free all memory blocks created */
  rgx_free();
}
