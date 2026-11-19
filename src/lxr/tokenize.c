

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

/*
.. _s_ <- transition of state _s_ by class _c_ (lxr_dead if none).
.. Look for the transition in [base, base + nclass) using check/next
.. tables; if it is not there, use the fallback (def). The transition
.. is DEAD once the number of fallbacks reaches lxr_max_depth.
.. (No meta class as of now.)
*/
#define lxr_next_state(_s_, _c_)                                     \
  do {                                                               \
    int _depth_ = 0;                                                 \
    while ( lxr_not_rejected (_s_) &&                                \
      ((int) lxr_check [lxr_base [_s_] + (_c_)] != (_s_)) )          \
      (_s_) = (_depth_++ == lxr_max_depth) ? lxr_dead :              \
        (int) lxr_def [_s_];                                         \
    if ( lxr_not_rejected (_s_) )                                    \
      (_s_) = (int) lxr_next [lxr_base [_s_] + (_c_)];               \
  } while (0)

/*
.. The main lexer function. Returns 0, when EOF is encountered. So,
.. don't use return value 0 inside any action.
*/
YY_DECL
{
  int state, class, acc_token, acc_len, token;
  #if lxr_eol_class
  int eol;
  #endif
  #ifdef LXR_YYLINENO
  unsigned char * cls;
  #endif

  #define lxr_tokenizer_init()                                       \
    do {                                                             \
      yytext [yyleng] = lxr_hold_char;                               \
      acc_len = (int) (lxr_bptr - lxr_start);                        \
      yytext += acc_len;                                             \
      lxr_start += acc_len;                                          \
      state = 1 + (yytext [-1] == '\n');                             \
      acc_token = acc_len = 0;                                       \
    } while (0)

  lxr_tokenizer_init();
  for (;;) {            /* Loop looking the longest token until EOF */

    /*
    .. Transition loop, until the automaton rejects. After consuming
    .. (lxr_bptr - lxr_start) bytes, the automaton is in "state". As
    .. in flex, the last accepting state seen so far is remembered in
    .. (acc_len, acc_token), so that the longest token is known when
    .. the automaton rejects. (In case two patterns match the longest
    .. token, the accept table already gives the first token defined
    .. in the lexer file.)
    */
    for (;;) {
      class = (int) *lxr_bptr;

      /*
      .. End of buffer : read more input and go on from the same state.
      .. (The bytes from lxr_start are kept, at the same offsets.)
      */
      if (class == lxr_eob_class) {
        lxr_buffer_update ();
        continue;
      }

      if ( (token = lxr_accept [state]) ) {
        acc_token = token;
        acc_len = (int) (lxr_bptr - lxr_start);
      }

      /*
      .. EOL anchor '$' : before a newline (or EOF), the EOL transition
      .. may accept a pattern like "abc$". Take it, unless a pattern
      .. defined before it already accepts at this position. (Skipped,
      .. if no pattern uses '$')
      */
      #if lxr_eol_class
      if ( class == lxr_nel_class || class == lxr_eof_class ) {
        eol = state;
        lxr_next_state (eol, lxr_eol_class);
        if ( lxr_not_rejected (eol) && (token = lxr_accept [eol]) &&
          ( !lxr_accept [state] || token < lxr_accept [state] ) ) {
          acc_token = token;
          acc_len = (int) (lxr_bptr - lxr_start);
        }
      }
      #endif

      lxr_next_state (state, class);
      if ( !lxr_not_rejected (state) )
        break;
      lxr_bptr++;
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
    .. Optional line & column tracking (as in flex, "%option yylineno"
    .. or LXR_YYLINENO). Newlines of the token are counted before its
    .. action runs. (Bytes read by lxr_input () are counted there.)
    */
    #ifdef LXR_YYLINENO
    if (acc_token != lxr_eof_accept) {
      yycolno = lxr_colno_next;
      for (cls = lxr_start; cls < lxr_bptr; ++cls) {
        if (*cls == lxr_nel_class) {
          yylineno++;
          lxr_colno_next = 1;
        }
        else
          lxr_colno_next++;
      }
    }
    #endif

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

        default :
          fprintf (stderr, "internal error : unknown accept state");
          exit (-1);

      }
    } while (0);

    lxr_tokenizer_init ();             /* start reading a new token */
  }

  return EOF;                     /* The code will never reach here */
}
