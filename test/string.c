/* reverse polish notation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"
#include "nfa.h"

int main () {
  /*
  .. actual regex is this. We need to add escape caracters to make it a C string
  ..       \"([^"\\\x00-\x1F]|\\["\\/bfnrt]|\\u[0-9a-fA-F]{4})*\"
  */
  char * rgxs [] =
    {
      "\"([^\\x00-\\x1F]|\\\\[\"\\/bfnrt]|\\\\u[0-9a-fA-F]{4})*\"",
      /*
      "\"(\\\\[\"\\/bfnrt])*\"",
      "(\\\\[\"\\/bfnrt])+",
      "\\\\[\"\\/bfnrt]",
      "\\\\t",
      "\\\\\\",
      "\\\\u[0-9a-fA-F]{4}",
      */
    };

  /*
  .. test cases
  */
  const char * samples [] =
    {
      "\"hello\"",
      "\"\\u012f add\"",
      "\"\\u012f\"",
      "\"\\t\\t\\n  \\uaaaaX\"",
      "\"\\t\\t\\n\"",
      "\"\\t\""
    };

  nfa_reset (rgxs, sizeof (rgxs) / sizeof (rgxs[0]));

  char buff[60];
  for (int r=0; r < sizeof (rgxs)/ sizeof (rgxs [0]); ++r) {

    int rpn [RGXSIZE];
    char * rgx = rgxs [r];
    State * nfa = NULL;
    printf ("\nRpn for rgx `%s`", rgx);
    if (rgx_rpn (rgx, rpn) < 0)
    {
      printf ("\nWrong rgx"); continue;
    } 
    rgx_rpn_print (rpn);
    int status = rgx_nfa (rgx, &nfa, 1);
    if (status < RGXEOE) 
      printf ("failed in creating NFAi for rgx %s: %d", rgx, status); 

    /* each test strings */
    for (int i=0; i< sizeof (samples) / sizeof (samples[0]); ++i) {

      const char * source = samples [i];
      do {
        int m = rgx_nfa_match (nfa, source);
        //if (m <= 0) printf ("failed in match check %d", m); 
        //source += m > 1 ? (m-1) 
        if (m > 0) {
          m -= 1; buff[m] = '\0';
          if(m) memcpy (buff, source, m);
          printf ("\nRegex %s: found in txt `%s` (`%s`)", rgx, buff, source);
        }
      } while (*++source);
    }
  }

  rgx_free();
}
