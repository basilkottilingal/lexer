#ifndef _RGX_NFA_H_
#define _RGX_NFA_H_

  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
  #include <assert.h>

  #include "regex.h"
  #include "allocator.h"
  #include "stack.h"

  enum {
    NFAEPS  = 256,  /* Epsilon/Empty transition */
    NFAACC  = 257,  /* Accepting state */
    NFAERR  = -2,   /* Unknown alphabet outside [0, 256) */
  };

  int  states_add        ( State * start, Stack * list, State *** buff );
  int  states_at_start   ( State * nfa,   Stack * list, State *** buff );
  int  states_transition ( Stack * from,  Stack * to,   State *** buff, int c );
  int  states_bstack     ( Stack * list,  Stack * bits );
  int  state_token       ( State * f );
  int  nfa_reset         ( char ** rgx, int nrgx );

  #define RGXMATCH(_s_) (_s_)->flag

#endif
