

/*
.. table based tokenizer. Assumes,
.. (a) state '0' : REJECT
..     state '1' is the starting dfa.
..     state '2' is the starting dfa (BOL)
.. (b) There is no zero-length tokens,
.. (c) maximum depth of 1 for "def" (fallback) chaining.
.. (d) Doesn't use meta class.
.. (e) Token value '0' : rejected. No substring matched
.. (f) accept value in [1, ntokens] for accepted tokens
*/

#define lxr_max_depth                        1
#define lxr_dead                             0
#define lxr_not_rejected(s)                  (s)          /* s != 0 */
#ifndef lxr_state_stack_size 
#define lxr_state_stack_size                 128
#endif
#define lxr_clear_stack()           stack_idx = lxr_state_stack_size

/*
.. The main lexer function. Returns 0, when EOF is encountered. So,
.. don't use return value 0 inside any action.
*/
YY_DECL
{
  static int states [lxr_state_stack_size];
  unsigned char * cls;
  int state, class, acc_token, acc_len, stack_idx,
    depth, token,
    #if lxr_eol_class
    eol,
    #endif
    acc_len_old, acc_token_old, state_old;

  #define lxr_tokenizer_init()                                       \
    do {                                                             \
      yytext [yyleng] = lxr_hold_char;                               \
      acc_len = (int) (lxr_bptr - lxr_start);                        \
      yytext += acc_len;                                             \
      for (; acc_len; --acc_len) {                                   \
        if (*lxr_start++ == lxr_nel_class) {                         \
          yycolno = 1;                                               \
          yylineno++;                                                \
          continue;                                                  \
        }                                                            \
        yycolno++;                                                   \
      }                                                              \
      state = 1 + (yytext [-1] == '\n');                             \
      acc_token = 0;                                                 \
      lxr_clear_stack();                                             \
    } while (0) 

  lxr_tokenizer_init();
  do {                  /* Loop looking the longest token until EOF */

    /* in case to back up */
    acc_len_old = acc_len;
    acc_token_old = acc_token;
    state_old = state;
    
    do {                            /* Transition loop until reject */
      class = (int) *lxr_bptr++;
      states [--stack_idx] = state;         /* Keep stack of states */

      /*
      .. find the transition corresponding to the class using check/
      .. next tables and if not found in [base, base + nclass), use
      .. the fallback.Note: (a) No meta class as of now. (b) assumes
      .. transition is DEAD if number of fallbacks reaches max_depth
      */
      depth = 0;
      while ( lxr_not_rejected (state) && 
        ((int) lxr_check [lxr_base [state] + class] != state) ) {
        state = (depth++ == lxr_max_depth) ? lxr_dead : 
          (int) lxr_def [state];
      }

      if ( lxr_not_rejected (state) )
        state = (int) lxr_next [lxr_base[state] + class];

    } while ( lxr_not_rejected (state) && stack_idx );

    cls = lxr_bptr - 1;
    while (stack_idx < lxr_state_stack_size) {
      /*
      .. (a) "acc_token" will be the longest token
      .. (b) In case two patterns are matched for the longest token,
      .. use the first token defined in the lexer file.
      .. (c) NOTE : in case no pattern use the EOL anchor '$', the
      .. following optional snippet will be skipped.
      */
      #if lxr_eol_class
      if ( *cls == lxr_eof_class || *cls == lxr_nel_class ) {
        eol = states [stack_idx];
        depth = 0;
        while ( lxr_not_rejected (eol) && 
          ((int) lxr_check [lxr_base [eol] + lxr_eol_class] != eol)) {
          eol = (depth++ == lxr_max_depth) ? lxr_dead : 
            (int) lxr_def [eol];
        }

        if ( lxr_not_rejected (eol) ) {
          eol = (int) lxr_next [lxr_base[eol] + lxr_eol_class];
          if ((token = lxr_accept [eol])
            && token < lxr_accept [states [stack_idx]])
          {
            acc_len = cls - lxr_start;
            acc_token = token;
            break;
          }
        }
      }
      #endif
      if ( (token = lxr_accept [states [stack_idx++]]) ) {
        acc_len = cls - lxr_start;
        acc_token = token;
        break;
      }
      --cls;
    }

    if (lxr_not_rejected (state)) {
      lxr_clear_stack ();
      continue;
    }

    /*
    .. Put the reading pointer at the last accepted location.
    .. Update with the new holding character.
    */
    yyleng = acc_len ? acc_len : (acc_len = 1);
    lxr_bptr = lxr_start + acc_len;
    lxr_hold_char = yytext [acc_len];
    yytext [acc_len] = '\0';

    /*
    .. handling accept/reject. In case of accepting, corresponding
    .. action snippet will be executed. In case of reject (unknown,
    .. pattern) lexer will simply consume without any warning. If you
    .. don't want it to go unnoticed in suchcases, you can add an "all
    .. catch" regex (.) in the last line of the rule section after all
    .. the expected patterns are defined. A sample lex snippet is
    ,, given below
    ..
    .. [_a-zA-Z][_a-zA-Z0-9]*                   { return ( IDENT );  }
    .. .                                        { return ( ERROR );  }
    */

    do {   /* Just used a newer block to avoid clash of identifiers */

      switch ( acc_token ) {
        case 0 :                                 /* Unknown pattern */
          #ifdef lxr_exception_fail
          fprintf (stderr, "unknown token");
          exit (-1);
          #endif
          break;

        /*% replace this line with case <token> : <action>  break; %*/

        case lxr_eof_accept :
          lxr_bptr = lxr_start;
          yyleng = 0;
          return 0;

        case lxr_eob_accept :
          lxr_bptr --;
          lxr_buffer_update ();
          break;

        default :
          fprintf (stderr, "internal error : unknown accept state");
          exit (-1);

      }
    } while (0);

    if (acc_token != lxr_eob_accept) {
      lxr_tokenizer_init ();           /* start reading a new token */
      continue;
    }

    /*
    .. In case of EOB, we have to restart from the last state before
    .. the lxr_eob_class transition. Reset acc_len and acc_token to
    .. their backup ( In case there is no accepting state in the stack
    .. "states [] ". )
    */
    state = (stack_idx == lxr_state_stack_size) ? 
      state_old : states [stack_idx];
    acc_len = acc_len_old;
    acc_token = acc_token_old;

  } while (1);

  return EOF;                     /* The code will never reach here */
}
