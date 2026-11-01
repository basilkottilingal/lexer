/* reverse polish notation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"
#include "nfa.h"

int main () {
  char * rgx[] = { "N[a-z1-3;]", "[^a-f?]|swww*", "^[ \\t][A-Z]", "\\\\u[0-9a-fA-F]{4}", "\\\\$", "[A-Za-z_$\\x80-\\xff][A-Za-z0-9_$\\x80-\\xff]*" };
  int rpn [RGXSIZE];

  int nrgx = sizeof (rgx) / sizeof (rgx[0]);
  nfa_reset (rgx, nrgx);
  State * nfa = NULL;

  for (int i=0; i< sizeof (rgx) / sizeof (rgx[0]); ++i) {
    int l = rgx_rpn (rgx[i], rpn);
    printf ("\nRpn for rgx \"%s\" [rpnlen %d]", rgx[i], l);
    rgx_rpn_print (rpn);
    int status = rgx_nfa (rgx[i], &nfa, 1);
    if (status < RGXEOE) 
      printf ("failed in creating NFAi for rgx %s: %d", rgx[i], status); 
  }

  rgx_free();
}
