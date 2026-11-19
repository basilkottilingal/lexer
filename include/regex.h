#ifndef _RGX_H_
#define _RGX_H_
  #include "stack.h"
  /*
  .. Warning !!!
  .. Regex reading functions doesn't take care of all regex rules and 
  .. patterns. Regex rules here are enough to define patterns for JSON
  .. lexicons, C lexicons.
  ..
  .. In case, to expand regex reader to create an exhuastive set, read
  .. https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_expressions/Cheatsheet
  ..
  .. If interested in implementing NFA and DFA, read
  .. https://swtch.com/~rsc/regexp/regexp1.html
  */

  /* 
  .. NFA and DFA State
  */
  typedef struct State {
    struct State ** out;
    int id, counter, ist, flag;
  } State ;  
  typedef struct DState DState;

  /*
  .. API
  .. (a) int rgx_match ( const char * rgx, const char * txt ) :
  ..      returns a integer value, say, val > 0 if the text, "txt" (or 
  ..      a starting substring of "txt") matches the regex pattern. 
  ..      The number of characters matched = val - 1.
  ..      If val = 0, then "txt" doesn't follow "rgx" pattern,
  ..      and val < 0 refers to some error.
  .. (b) int rgx_dfa ( const char * rgx, DState ** dfa );
  ..      For repeated match lookup of a regex "rgx", this function will
  ..      evaluate minimised DFA in dfa[0].
  ..      The return value < 0 means an error.
  .. (c) int rgx_dfa_match ( DState * dfa, const char * txt);
  ..      Similar to "rgx_match", but uses minimal "dfa" for "rxg"
  .. (d) int rgx_free ();
  ..      Free allocated memory blocks.
  .. (e) flush all reported error (if any) to stderr.
  */
  int  rgx_match     ( /*const*/ char * rgx, const char * txt );
  int  rgx_dfa       ( /*const*/ char * rgx, DState ** dfa );
  int  rgx_dfa_match ( DState * dfa,     const char * txt);
  void rgx_free      ( );
  void errors        ( );
  int  dfa_tables    ( int ***, int ** );

  /*
  .. Lower level or internal api. Maybe used for debug
  .. (a) creates and NFA. Warning : Should use after nfa_reset() or
  ..      expect a segfault.
  .. (b) see if "txt" matches regex "rgx" using the nfa created
  .. (c) Evaluate minimised DFA in  dfa[0] for a list of regular 
  ..      expressions "rgx_list".
  ..      List should have the regex ordered int the decreasing preference,
  ..      in case of multiple regex are satisfied by "txt".
  .. (d) report an error and stack it to the list of error.
  */
  int  rgx_nfa       ( char   * rgx, State ** nfa, int itoken );
  int  rgx_nfa_match ( State  * nfa, const char * txt );
  int  rgx_list_dfa  ( char ** rgx, int nr, DState ** dfa );
  int  rgx_lexer_dfa ( char ** rgx, int nr, DState ** dfa );
  void error         ( const char * err, ... );
  int  dfa_eol_used  ( );

  enum RGXFLAGS {
    RGXEOE  = -1,   /* "End of expression" : Regex parsed successfully */
    RGXOOM  = -10,  /* "Out of Memory" : A stack ran out of space */
    RGXERR  = -11,  /* "Error" : Unknown/Non-implemented regex pattern*/
  };

  /*
  .. RGXSIZE is the default size of stack for rgx literals/NFA states/ etc
  .. If an implementation fail with RGXOOM flag, recompile with 
  .. larger RGXSIZE
  */
  #ifndef RGXSIZE
    #define RGXSIZE 256
  #endif

  /*
  .. Maximum count allowed in quantifiers x{m,n}. It has to be < 256
  .. so that the counts stored in rpn are never confused as operators
  */
  #ifndef RGXMAXREP
    #define RGXMAXREP 255
  #endif

  #define   RGXOP(_c_)     ((_c_) | 256)
  #define ISRGXOP(_c_)     ((_c_) & 256)

  /* Lower level/Internal funtions. Maybe used for debug */
  int rgx_rpn       ( char * rgx, int * rpn );
  int rgx_rpn_print ( int * rpn );

#endif
