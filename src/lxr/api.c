

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
static char lxr_yytext_dummy[] = "\n";

/*
.. yytext and yyleng are respectively the token text and token length
.. of the last accepted token
*/
char * yytext = & lxr_yytext_dummy [1];
int    yyleng = 0;

/*
.. Current line number and columen number.
*/
int yylineno = 1;
int yycolno = 1;
