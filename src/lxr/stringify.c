
/*
  Table based lexer auto-generated using the lexer generator
  [https://github.com/basilkottilingal/lexer].


  Copyright (C) [2026] [Ahmed Basil KOTTILINGAL]

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

# line 2 "stringify.l"
/*
.. convert all .c files into const string [] literals.
*/

#define YY_DECL   int yylex ()

#include <stdio.h>
#include <assert.h>
#include <string.h>


# line 37 "stringify.c"

/*
.. Equivalence classes for alphabets in [0x00, 0xFF] are
.. stored in the table lxr_class []. The number of equivalence
.. classes also counts the special equivalence classes like
.. that of EOB/EOL/EOF/BOL. However BOL transition is never
.. used, as the state 2 is reserved for the starting DFA in
.. case of BOL status. lxr_eol_class will be set to 0 in case
.. no patterns are found with EOL requirement.
*/
#define lxr_nclass        15          /* num of eq classes */
#define lxr_nel_class      5          /* new line  '\n'    */
#define lxr_eob_class     11          /* end of buffer     */
#define lxr_eol_class      0          /* end of line       */
#define lxr_eof_class     13          /* end of file       */

/*
.. States [1, 11] are reserved for tokens listed in the lex 
.. source file. In addition, the following two accept states
.. are used internally for EOF/EOB
*/
#define lxr_eof_accept    12          /* end of file       */
#define lxr_eob_accept    13          /* end of buffer     */

static short lxr_check [28] = {
    2,

    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
    2,   13,    2,    3,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0
};

static short lxr_next [28] = {
   13,

    3,    6,    8,    5,    4,    7,   10,    9,   11,   12,
   15,   15,   14,    3,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0
};

static short lxr_base [16] = {
    0,

    0,    0,   13,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    1,    0,    0
};

static short lxr_accept [16] = {
    0,

    0,    0,    1,    2,    3,    4,    5,    6,    7,    8,
    9,   10,   11,   12,   13
};

static short lxr_def [16] = {
    0,

    2,    0,   13,   13,   13,   13,   13,   13,   13,   13,
   13,   13,   14,   15,    0
};

static unsigned char lxr_meta [15] = {
    0,

    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0
};

static unsigned char lxr_class [256] = {
    0,

    0,    0,    0,    0,    0,    0,    2,    3,    4,    5,
    6,    7,    8,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    1,    1,    9,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,   10,    1,    1,    1,    1,    1,    1,    1,    1,

    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,

    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
.. User can define the signature for lexer function as a macro
.. The default type is `int lxr_lex (void)`
*/
#ifndef YY_DECL
  #define YY_DECL  int lxr_lex ()
#endif

/*
.. The list of api defined in lxr
.. - lxr_source ( const char * source ) : sets "source" as the input
..     source for the lexer.
.. - lxr_read_bytes ( const char * bytes, size_t len, int bol ) : 
       sets "bytes []" as the input source.
..     "len" is the size of bytes[]. Input non-zero value for "bol" if
..     the first character from bytes[] satisfy BOL status. Assumes
..     the last character of bytes[] is followed by EOF.
..     Warning : expects "bytes []" is not modified in the middle of
..     parsing.
.. - lxr_input () : return the next byte from the input source and
..     move the reading pointer forwards. Return value in [0x00, 0xFF]
..     or EOF (-1)
.. - lxr_unput () : return previous byte from the input source and
..     move the reading pointer backwards, in [0x00, 0xFF].
..     Return value in [0x00, 0xff] or error (-1) if "limit" reached.
..     "limit" : You can unput a maximum of yyleng characters (and the
..     recently emulated lxr_input () characters).
.. - lxr_token () : in case input()/unput() moved the pointer, you can
..     call lxr_token() to accept a new token wherever the pointer is.
.. - lxr_clean () : clean the stack of buffers.
..     Call at the end of the program
*/

void lxr_source     ( const char * source );
void lxr_read_bytes ( const char * bytes, size_t len, int bol );
int  lxr_input      ( );
int  lxr_unput      ( );
void lxr_token      ( );
void lxr_clean      ( );

/*
.. If user hasn't defined alternative to malloc, realloc & free.
.. Note : In case user, defines alternative macros, please make sure
.. to use the signature of the malloc/realloc/free functions in stdlib
*/
#ifndef LXR_ALLOC
  #define lxr_alloc(_s_)        malloc  (_s_)
  #define lxr_realloc(_a_,_s_)  realloc (_a_, _s_)
  #define lxr_free(_a_)         free (_a_)
#endif
  
#ifndef LXR_BUFF_SIZE
  #define LXR_BUFF_SIZE  1<<14       /* default buffer size : 16 kB */
#endif
static size_t lxr_size = LXR_BUFF_SIZE;

/*
.. Input stream of bytes on which tokenization happens. If no file or
.. bytes are specified, stdin is assumed as the source of bytes.
.. You can specify a file as input source using "lxr_source ()" or
.. a bytes array as the input source using "lxr_read_bytes ()"
*/
#define lxr_source_is_stdin   0
#define lxr_source_is_file    1
#define lxr_source_is_bytes   2
static int lxr_source_type    = lxr_source_is_stdin;
static char * lxr_infile      = NULL;
static FILE * lxr_in          = NULL;
static const char * lxr_bytes_start = NULL;
static const char * lxr_bytes_end   = NULL;

static void lxr_buffer_update ();
static char lxr_yytext_dummy [3] = { '\n', '\0', '\0' };

/*
.. yytext and yyleng are respectively the token text and token length
.. of the last accepted token
*/
char * yytext = & lxr_yytext_dummy [1];
int    yyleng = 0;

/*
.. Line and column numbers. They are updated only if line tracking is
.. enabled ("%option yylineno" in the lex file, or LXR_YYLINENO defined),
.. as in flex. Otherwise they stay at 1.
.. - yylineno : as in flex. Inside an action, it already counts the
..   newlines of the current token (and those read with lxr_input ()).
.. - yycolno  : (not in flex) column where the current token starts.
*/
int yylineno = 1;
int yycolno = 1;
#ifdef LXR_YYLINENO
static int lxr_colno_next = 1;      /* column of the next unread byte */
#endif


/*
.. ---------------------------- Lexer --------------------------------
.. This is a generated file for table based lexical analysis. The file
.. contains (a) functions and macros related to reading source code
.. and running action when a lexicon detected (b) compressed table for
.. table based lexical analysis.
*/

typedef struct lxr_buff_stack {
  char * bytes;
  size_t size;
  struct lxr_buff_stack * next; 
} lxr_buff_stack ;

static unsigned char
  lxr_dummy[4] = {lxr_eob_class, lxr_eob_class, lxr_eob_class, 0};
static unsigned char * lxr_start = lxr_dummy;
static unsigned char * lxr_bptr  = lxr_dummy;
static unsigned char * lxr_class_buff = NULL;
static size_t lxr_class_buff_size = 0;

static char lxr_hold_char = '\0';

#define lxr_reset() do {                                             \
    lxr_start = lxr_bptr = lxr_dummy;                                \
    lxr_hold_char = '\0';                                            \
    yytext = & lxr_yytext_dummy [1];                                 \
    yyleng = 0;                                                      \
  } while (0)

static lxr_buff_stack * lxr_buff_stack_current = NULL;

void lxr_source (const char * in) {
  /* fixme : warn in case "bytes[]" is being used */
  if (lxr_in) {
    fprintf (stderr, "cannot change source file in the middle."
      "\nuse lxr_stack_push (source) to change source file."
      "\nalternatively use lxr_clear() + lxr_source()");
    exit (-1);
  }
  if (in == NULL || (lxr_in = fopen (in, "r")) == NULL) {
    fprintf (stderr, "cannot find lxr_infile %s", in ? in : "");
    fflush (stderr);
    exit (-1);
  }
  lxr_infile = strdup (in);
  lxr_source_type = lxr_source_is_file;
}

void lxr_read_bytes (const char * bytes, size_t len, int eob) {
  /* fixme : in case another byte source available */
  size_t size = len < lxr_size ? len : lxr_size;
  lxr_buff_stack * bf = lxr_alloc (sizeof (lxr_buff_stack));
  char * b = lxr_alloc (size + 3);
  if (lxr_class_buff_size < size)
    lxr_class_buff_size = size;
  lxr_class_buff =
    lxr_realloc (lxr_class_buff, lxr_class_buff_size + 3);
  if ( bf == NULL || b == NULL || lxr_class_buff == NULL ) {
    fprintf (stderr, "lxr_realloc failed");
    exit (-1);
  }
  bf->bytes = b;
  bf->size  = size;    /* used when the buffer is grown (realloc) */
  bf->next = lxr_buff_stack_current;
  lxr_buff_stack_current = bf;
  b [0] = eob ? '\n' : '\0';     /* yytext [-1] : decides BOL status */
  yytext = & b [1];
  yyleng = 0;
  memcpy (yytext, bytes, size);
  unsigned char * ub = (unsigned char *) yytext, 
    * cls = lxr_class_buff;
  for (size_t i=0; i<size; ++i)
    *cls++ = lxr_class [*ub++];
  cls [0] = cls [1] =
    (size < len ? lxr_eob_class : lxr_eof_class);
  lxr_bytes_start = & bytes [size];
  lxr_bytes_end   = & bytes [len];
  lxr_hold_char   = * yytext;
  lxr_bptr = lxr_start = lxr_class_buff;
  *yytext = '\0';
  lxr_source_type = lxr_source_is_bytes;
}

/*
.. In case input() or unput () was called after accepting the last
.. token, create a new token depending on the pointer
*/
void lxr_token () {
  if ( yyleng == (int) (lxr_bptr - lxr_start) )
    return;
  yytext [yyleng] = lxr_hold_char;
  yyleng = (int) (lxr_bptr - lxr_start);
  lxr_hold_char = yytext [yyleng];
  yytext [yyleng] = '\0';
}

void lxr_clean () {
  /*
  .. Free all the memory blocks created for buffer.
  .. User required to run this at the end of the program
  */
  lxr_buff_stack * s = lxr_buff_stack_current, * next;
  while (s != NULL) {
    next = s->next;
    free (s->bytes);
    free (s);
    s = next;
  }
  lxr_buff_stack_current = NULL;

  if (lxr_class_buff) {
    free (lxr_class_buff);
    lxr_class_buff = NULL;
    lxr_class_buff_size = 0;
  }

  if (lxr_in)
    fclose (lxr_in);
  if (lxr_infile)
    free (lxr_infile);
  lxr_in = NULL;
  lxr_infile = NULL;
  lxr_reset ();
}

/*
.. return next (unsigned) byte from input stream. Return value is in
.. [0x00, 0xFF]. Exception EOF
*/
int lxr_input () {
  if (*lxr_bptr == lxr_eob_class) {
    yytext [yyleng] = lxr_hold_char;
    lxr_buffer_update ();
    lxr_hold_char = yytext [yyleng];
    yytext [yyleng] = '\0';
  }

  if (*lxr_bptr == lxr_eof_class)
    return EOF;

  size_t idx = lxr_bptr++ - lxr_start;
  int c = (int) (unsigned char)
   ( ((size_t) yyleng == idx) ? lxr_hold_char : yytext [idx] );
  #ifdef LXR_YYLINENO
  if (c == '\n') {
    yylineno++;
    lxr_colno_next = 1;
  }
  else
    lxr_colno_next++;
  #endif
  return c;
}

/*
.. undo the lxr_input(). return previous byte.
.. Note : there is a limit on how many unput() can be called. You
.. cannot unput beyond the "yytext" pointer, i.e, you cannot go to
.. the last accepted token.
*/ 
int lxr_unput () {
  if ( (size_t) (lxr_bptr - lxr_start) > 0) {
    size_t idx = (--lxr_bptr) - lxr_start;
    int c = (int) (unsigned char)
      ( ((size_t) yyleng == idx) ? lxr_hold_char : yytext [idx] );
    #ifdef LXR_YYLINENO
    /*
    .. as in flex, un-reading a newline decrements yylineno. (The column
    .. after un-reading a newline is not known : it is left unchanged)
    */
    if (c == '\n')
      yylineno--;
    else if (lxr_colno_next > 1)
      lxr_colno_next--;
    #endif
    return c;
  }
  return EOF;               /*error : cannot undo beyond last token */
}

/*
.. internal function to update buffer, when EOB transition is hit
*/
static void lxr_buffer_update () {

  if (lxr_in == NULL) {
    lxr_in = stdin;
    lxr_infile = strdup ("<stdin>");
  }

  if (lxr_bptr [0] != lxr_eob_class ||
      lxr_bptr [1] != lxr_eob_class)
  {
    fprintf (stderr, "lxr_buffer_update () : internal check failed");
    exit (-1);
  }
    
  /*
  .. "non_parsed" : Bytes already consumed by automaton but yet
  .. to be accepted. These bytes will be copied to the new buffer.
  .. fixme : may reallocate if non_parsed is > 50 % of buffer.
  .. (as of now, reallocate if non_parsed == 100% of buffer)
  */
  size_t size = lxr_size, non_parsed = lxr_bptr - lxr_start;

  lxr_buff_stack * s = lxr_buff_stack_current;
  if (s == NULL || (yytext - s->bytes) > 1 ) {

    while ( size <= non_parsed )
      size *= 2;

    s = lxr_alloc (sizeof (lxr_buff_stack));
    if ( s == NULL ) {
      fprintf (stderr, "lxr_alloc failed");
      exit (-1);
    }
    * s = (lxr_buff_stack) {
      .size  = size,
      .bytes = lxr_alloc (size + 3),
      .next  = lxr_buff_stack_current
    };
    if (lxr_class_buff_size < size) {
      lxr_class_buff_size = size;
      lxr_class_buff = lxr_realloc (lxr_class_buff, size + 3); 
    }
    if ( s->bytes == NULL || lxr_class_buff == NULL ) {
      fprintf (stderr, "lxr_alloc failed");
      exit (-1);
    }
    memcpy (s->bytes, & yytext [-1], non_parsed + 1);
    memmove (lxr_class_buff, lxr_start, non_parsed);

    lxr_buff_stack_current = s; 
  }
  else {
    size = s->size * 2;
    s->bytes = lxr_realloc (s->bytes, size + 3);
    if (lxr_class_buff_size < size) {
      lxr_class_buff_size = size;
      lxr_class_buff = lxr_realloc (lxr_class_buff, size + 3); 
    }
    s->size  = size;
    if ( s->bytes == NULL || lxr_class_buff == NULL ) {
      fprintf (stderr, "lxr_realloc failed");
      exit (-1);
    }
  }

  yytext = s->bytes + 1;
  lxr_start = lxr_class_buff;
  lxr_bptr = lxr_start + non_parsed;
  lxr_hold_char = *yytext;
  
  size_t bytes;
  switch (lxr_source_type) {
    case lxr_source_is_stdin :
    case lxr_source_is_file  :
      bytes = 
        fread ( & yytext [non_parsed], 1, size - non_parsed, lxr_in );
      break;
    case lxr_source_is_bytes :
      bytes =
        (size_t) (lxr_bytes_end - lxr_bytes_start) <=
        (size - non_parsed) ? 
        (size_t) (lxr_bytes_end - lxr_bytes_start) :
        (size - non_parsed) ;
      memcpy ( & yytext [non_parsed], lxr_bytes_start, bytes );
      lxr_bytes_start += bytes;
      break;
    default :
      fprintf (stderr, "lxr internal error: lxr_source_type unknown");
      exit (-1);
  }

  unsigned char end_class = lxr_eob_class;
  if (bytes < size - non_parsed) {
    if (! ((lxr_source_type == lxr_source_is_bytes) ? 
           (lxr_bytes_start == lxr_bytes_end) : feof (lxr_in)) )
    {
      fprintf (stderr, "lxr buffer : fread failed !!");
      exit (-1);
    }
    end_class = lxr_eof_class;
  }
  unsigned char * byte = & ((unsigned char *) yytext) [non_parsed] ;
  for (size_t i=0; i<bytes; ++i)
    lxr_bptr [i] = lxr_class [ *byte++];
  lxr_bptr [bytes] = lxr_bptr [bytes + 1] = end_class;
  yytext   [bytes + non_parsed] = '\0';

}


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

        
        case 1 :
          # line 15 "stringify.l"
          {printf ("%s", yytext); }
          break;

        case 2 :
          # line 16 "stringify.l"
          { printf ("\\n\"\n\t\""); }
          break;

        case 3 :
          # line 17 "stringify.l"
          { printf ("\\t"); }
          break;

        case 4 :
          # line 18 "stringify.l"
          { printf ("\\a"); }
          break;

        case 5 :
          # line 19 "stringify.l"
          { printf ("\\v"); }
          break;

        case 6 :
          # line 20 "stringify.l"
          { printf ("\\b"); }
          break;

        case 7 :
          # line 21 "stringify.l"
          { printf ("\\r"); }
          break;

        case 8 :
          # line 22 "stringify.l"
          { printf ("\\f"); }
          break;

        case 9 :
          # line 23 "stringify.l"
          { printf ("\\\""); }
          break;

        case 10 :
          # line 24 "stringify.l"
          { printf ("\\\\"); }
          break;

        case 11 :
          # line 25 "stringify.l"
          { assert (0 && "character not allowed in c string literals"); }
          break;


# line 752 "stringify.c"

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

# line 28 "stringify.l"


int main (int argc, const char ** argv)
{
  if (argc < 2)
  {
    fprintf (stderr, "\nusage : ./stringify file1.c file2.c ...");
    exit (-1);
  }

  for (int iarg = 1; iarg <argc; ++iarg)
  {
    /* expects trailing '.c' */
    const char * file = argv [iarg], * dot = NULL, * nextdot = strchr (file, '.');
    while (nextdot)
    {
      dot = nextdot;
      nextdot = strchr (dot + 1, '.');
    }
    assert (dot && dot [1] == 'c' && dot [2] == '\0' && (int) (dot - file) > 0);
    while ( --dot != file )
      if (*dot == '/') /* strip file path */
      {
        file = dot + 1;
        break;
      }
    printf ("char stringify_");
    char c;
    while ( (c = *file++) != '.')
      printf ("%c", c);
    printf (" [ ] = \n\t\"");

    /* set argv [1] as the lexer source */   
    lxr_source (argv [iarg]);

    while (yylex ()) {};
    printf ("\";\n\n");

    lxr_clean ();
  }

}
