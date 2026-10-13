/*
.. Read a lex file. Follows the flex's ".lex" format. You can refer to
.. the flex documentation on the input file format here
.. https://westes.github.io/flex/manual/Format.html#Format
.. Input file need to follow the format
..
.. definitions
.. %%
.. rules
.. %%
.. user code
..
flex file.l*/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "stack.h"
#include "allocator.h"
#include "lexer.h"
#include "regex.h"
#include "class.h"

#include "lxr/stringified.h"

static int isdebug = 0;
void lxr_debug () {
  isdebug = 1;
}

static FILE * out = NULL;
static FILE * in = NULL;

static char * infile = NULL;
static char * outfile = NULL;

static int line_out = 1;

static int echo ( char * buff) {
  size_t len = strlen (buff);
  if (fwrite (buff, 1, len, out) < len) {
    error ("echo() failed");
    return RGXERR;
  }
  while ( *buff )
    if ( *buff++ == '\n' )
      line_out++;
  return 0;
}

void echo_line ( char * source, int line ) {
  line_out += 2;
  fprintf (out,"\n# line %d \"%s\"\n",
    source ? line : line_out,
    source ? source : outfile );
}

/*
.. Flex macro that holds a regex pattern. Macro naming should follow
.. the format [_a-zA-Z][_a-zA-Z0-9]*. You can recurse macro, i.e a
.. macro can use a previous macro, in it's definition. Macros are
.. defined in the definition section. Examples of macros are below
.. 
.. ALPH   [_a-zA-Z]
.. DIGITS [0-9]
.. ID     {ALPHA}({ALPH}|{DIGITS})*
*/

typedef struct Macro {
  char * key, * rgx;
  struct Macro * next;
} Macro;

/*
.. A code snippet is defined in the definitions section in either of
.. the two forms as given below
.. 
.. %{
..   #include <stdio.h>
..   #include "func.h"
.. %}
*/

typedef struct Snippet {
  char * code;
  int line;
  struct Snippet * next;
} Snippet;

static Snippet * snippet_head = NULL;
static Snippet ** snippets = & snippet_head;

/*
.. patterns are defined exclusively in the rules section, that falls
.. in between two %% separators. The pattern should be followed by the
.. corresponding action. Pattern is defined in the POSIX extended
.. regular expressions (ERE) with few exceptions
.. (a) strings are allowed in quotes
.. (b) macros inside {}
.. (c) . (dot or "all-character" class) excludes '\n'
.. (d) anchors '^' and '$' are allowed only at the beginning/end of
..     the tokens respectively 
..
.. %%
.. ID       { return IDENTIFIER; }
.. %%
*/
typedef struct Action {
  char * rgx, * action;                    /* Pattern - Action pair */
  int line;
} Action;

static Stack * actions = NULL;

/* ...................................................................
.. ...................................................................
.. ..................  hash table of macros  .........................
.. ...................................................................
.. .................................................................*/

#define prime 67u
static Macro * table [prime];

static inline 
uint32_t hash ( const char * key, uint32_t len ) {
  	
  /* 
  .. Seed is simply set as 0
  */
  uint32_t h = 0;
  uint32_t k;

  /*
  .. Dividing into blocks of 4 characters.
  */
  for (size_t i = len >> 2; i; --i) {
    memcpy(&k, key, sizeof(uint32_t));
    key += sizeof(uint32_t);

    /*
    .. Scrambling each block 
    */
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;

    h ^= k;
    h = ((h << 13) | (h >> 19)) * 5 + 0xe6546b64;
  }

  /*
  .. tail characters.
  .. This is for little-endian.  You can find a different version 
  .. which is endian insensitive.
  */  
  k = 0; 
  switch (len & 3) {
    case 3: 
      k ^= key[2] << 16;
    case 2: 
      k ^= key[1] << 8;
    case 1: 
      k ^= key[0];
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
  }

  /* 
  .. Finalize
  */
  h ^= len;
  h ^= (h >> 16);
  h *= 0x85ebca6b;
  h ^= (h >> 13);
  h *= 0xc2b2ae35;
  h ^= (h >> 16);

  /* return murmur hash */
  return h;
}

/*
.. lookup for a macro in the macro hashtable "table"
*/
static Macro ** lookup (const char * key) {
  uint32_t h = hash (key, strlen (key));
  Macro ** p = & table [h % prime], * m;
  while ( (m = *p) != NULL ) {
    if ( !strcmp (m->key, key) )
      return p;
    p = & m->next;
  }
  return p;
}

/* ...................................................................
.. ...................................................................
.. .....  reading definition section ( macros & header code )  .......
.. ...................................................................
.. .................................................................*/


static int line = 0;

/*
.. Read a new macro and insert it into the hashtable and corresponding
.. regex pattern
*/
static int macro_new (char * macro, char * pattern) {

  #define COPY(start) do {                                           \
      size_t l = ptr - start;                                        \
      if (l + len >= RGXSIZE) {                                      \
        error ("rgx buffer out of memory");                          \
        return RGXERR;                                               \
      }                                                              \
      memcpy (& rgx [len], start, l);                                \
      len += l;                                                      \
    } while (0)

  Macro ** p, * m;
  char rgx [RGXSIZE], * ptr = pattern, c;
  size_t len = 0;

  while ( (c = *ptr++) ) {

    char * start = ptr-1;
    if (c == '\\') {
      if ( (c = *ptr++) == '\0' ) {
        error ("incomplete escaping");
        return RGXERR;
      }
      COPY (start);
      continue;
    }
    if (c != '{') {
      COPY (start);
      continue;
    }
    if ( (c = *ptr++) == ',' || ( c <= '9' && c >= '0' ) ) {
      COPY (start);
      continue;
    }

    char * child = ptr - 1;
    if ( !(c == '_' || (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z')) )
      return RGXERR;

    do {
      /*
      .. In case the macro uses another macro, substitute the child
      .. macro by the corresponding regex
      */
      if ( c == '}' ) {
        ptr[-1] = '\0';
        p = lookup (child);
        ptr[-1] = '}';
        if ( (m = *p) == NULL ) {
          error ("macro {%s} not found", child);
          return RGXERR;
        }
        size_t l = strlen (m->rgx);
        if (l + len >= RGXSIZE) {
          error ("rgx buffer out of memory"); 
          return RGXOOM;
        }
        memcpy ( & rgx [len], m->rgx, l );
        len += l;

        break;
      }

      if ( c == '_' || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') )
        continue;

      error ("invalid child macro name");
      return RGXERR;

    } while ( (c = *ptr++) );

    if (!c) {
      error ("incomplete }");
      return RGXERR;
    }
  }

  /*
  .. Check the validity of the macro's regex pattern before inserting
  .. to the table
  */
  rgx [len] = '\0';
  int rpn [RGXSIZE];
  len = rgx_rpn (rgx, rpn);       /* return rgx length if succesful */
  if (len <= 0) {
    error ("cannot create rpn for regex %s in line %d", rgx, line);
    return RGXERR;
  }
  rgx [len] = '\0';        /* Remove trailing white spaces (if any) */

  /*
  .. Insert macro to hashtable
  */
  p = lookup (macro);
  if ( (m = *p) != NULL ) {
    error ( "macro %s redefined in lexer definition in line %d",
      macro, line);
    return RGXERR;
  }
  *p = m = allocate (sizeof (Macro));
  m->key = allocate_str (macro);
  m->rgx = allocate_str (rgx);

  if ( ! ( (rgx [0] == '[' && rgx [len-1] == ']') ||
       (rgx [0] == '(' && rgx [len-1] == ')') ) ) {
    error ("warning : paranthesis, ( ), suggested for macro %s", macro);
  }
  
  return 0;
}

/*
.. Read definition section in the lexer file. The beginning section
.. until first ^%%, is called the definition where you define regex
.. macros and pre-lexer code snippets
*/
int lex_read_definitions () {

  char buff [PAGE_SIZE], *ptr, c;
  int code = 0, comment = 0;

  while (fgets (buff, sizeof buff, in)) {       /*read line by line */

    line++;

    size_t len = strlen (buff);

    if ( buff [ len-1 ] != '\n' && !feof (in) ) {
      error ("very long line");
      return RGXOOM;
    }

    if (code) {                   /* .. Source code section ^%{ ^%} */
      if ( buff [0] == '%' && buff [1] == '}' ) {
        code = 0;
        continue;
      }
      Snippet * s = allocate ( sizeof (Snippet) );
      s->code = allocate_str (buff);
      s->line = line;
      *snippets = s;
      snippets = & s->next;
      continue;
    }

    ptr = buff; c = *ptr++;
    if ( c == '/' && !comment ) {  /* See if beginning of a comment */
      if ( (c = *ptr++) != '*' ) {
        error ("unknown syntax");
        return RGXERR;
      }
      c = *ptr++; comment = 1;
    }
    if (comment || c == ' ' || c == '\t' || c == '\n') {
      do {         /* consume comments, white spaces and empty line */
        if (c == '\n')
          break;
        if (comment) {
          if (c == '*' && *ptr++ == '/') comment = 0;
          continue;
        }
        if ( c == '/' && *ptr == '*' ) {
          ++ptr;
          comment = 1;
          continue;
        }
        if (c == ' ' || c == '\t')
          continue;

        error ("content should be placed at BOL");
        return RGXERR;
      } while ( (c = *ptr++) );
      continue;
    }

    if ( c == '%' ) {
      c = *ptr++;
      if (c == '{' ) {      /* %{ is the beginning of code snippets */
        code = 1;
        continue;
      }
      if (c == '%')          /* %% is the end of definition section */
        return 0;
      error ("warning : unknown/unimplemented in line %d", line);
      continue;  /* Other % option, %top {}, etc .. not implemented */
    }

    /*
    .. A macro definition, maps a macro identifier to a rgx fragment.
    */
    if ( c == '_' || (c <='z' && c >= 'a') ||
       (c <='Z' && c >= 'A') ) { 
      while ( (c = *ptr++) != '\n' ) {
        if ( c == '_' || (c <='z' && c >= 'a') ||
          (c <='Z' && c >= 'A') || (c <='9' && c >= '0') )
          continue;
        if ( ! (c == ' ' || c == '\t' ) ) {
          error ("missing separator");
          return RGXERR;
        }
        ptr [-1] = buff [len - 1] = '\0';
        while ( (c = *ptr++) == ' ' || c == '\t' ){}
        --ptr;
        int status = macro_new (buff, ptr);
        if (status < 0) {
          error ("cannot create macro %s", buff);
          return RGXERR;
        }
        break;
      }
      if ( c == '\n' ) {
        error ("ill defined macro");
        return RGXERR;
      }
      continue;
    }

    /*
    .. Anything other than (i) %**, (ii) macro definition, (iii) empty
    .. white spaces and/or comments are taken as unknown syntax
    */
    error ("unknown syntax");
    return RGXERR;

  }

  error ("Did not encounter %%");
  return RGXERR;
}

/* ...................................................................
.. ...................................................................
.. ........  reading rule section ( rules & action )  ................
.. ...................................................................
.. .................................................................*/

/*
.. Naive way to look for the regex pattern at the beginning of the
.. line. The regex will be stored in the buffer "rgx".
*/
static
int _pattern (char * rgx, size_t lim) {

  #define MACROLIM 128
  #define EMPTY    10
  #define NOACTION 11

  int c, len = 0, escape = 0, charclass = 0, quote = 0, end = 0;
  char macro [MACROLIM];

  while ( (c = fgetc(in)) != EOF ) {

    rgx[len++] = (char) c;
    if (len == lim - 2) {
      error ( "rgx buff limit" );
      return RGXOOM;
    }
    if (escape) {
      escape = 0;
      continue;
    }
    if (c == '\\') {
      escape = 1;
      continue;
    }
    if (quote) {
      switch (c) {
        case '"' :
          len--;
          quote = 0;
          break;
        case '|' : case '*' : case '?' : case '+' :
        case '[' : case ']' : case '{' : case '}' :
        case '(' : case ')' : case '^' : case '$' :
        case '-' : case '.' : case '\\' :
          rgx [len-1] = '\\';
          rgx [len++] = c;
          break;
        default :
      }
      continue;
    }
    if (c == '\n') {

      ++line;
      if ( len == 1 )
        return EMPTY;                                 /* Empty line */
      rgx[--len] = '\0';
      return NOACTION;                    /*regex with no action !! */
    }
    if (charclass) {
      if (c == ']')
        charclass = 0;
      continue;
    }

    switch (c) {    /* outside quotes "" and outside character class*/
      case '/' :
        error ("warning : (in case) look ahead operator '/' is "
          "not implemented. line %d", line);
        break;
      case '[' : charclass = 1; break;
      case ' ' :
      case '\t' :          /* White space are taken as end of regex */
        rgx[--len] = '\0';
        if ( len == 0 ) {
          int comment = 0, p = c;
          while ( (c = fgetc(in)) != EOF ) {
            if ( c == '\n' ) { 
              ++line;
              if (!comment) break;
            }
            if ( comment ) {
              if (c == '/' && p == '*' ) comment = 0;
            }
            else if ( c == '/' ) {
              if ( (c = fgetc (in)) != '*' )
                return RGXERR;
              comment = 1;
            }
            else if ( !(c == ' '|| c == '\t') )  {
              return RGXERR;
            }
            p = c;
          }
          if ( c == EOF )
            return RGXERR;
          return EMPTY;       /* Empty line, with just white spaces */
        }
        #if 0
        printf ("\npattern  %s", rgx); fflush (stdout);
        #endif
        return 0;
      case '%' :
        if (len == 2 && end)
          return EOF;
        if (len == 1) end = 1;
        break;
      case '{' :
        --len;
        int k = 0;
        c = fgetc (in);
        if (c == ',' || (c >= '0' && c <= '9')) {
          rgx [len++] = '{';
          rgx [len++] = c;
          continue;
        }

        if ( !(c == '_' || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z')) )
          return RGXERR;

        do {
          if (c == '}')
            break;
          if (k == MACROLIM-2)
            return RGXOOM;
          if ( c == '_' || (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') )
            {
              macro [k++] = c;
              continue;
            }
          return RGXERR;
        } while ( (c = fgetc (in)) != EOF );

        if ( c!= '}')
          return RGXERR;

        macro [k] = '\0';
        Macro ** p = lookup (macro), *m;
        if ( (m = *p) == NULL ) {
          error ("macro {%s} not found", macro);
          return RGXERR;
        }
        int l = strlen (m->rgx);
        if (l + len >= lim) {
          error ("rgx buffer out of memory"); 
          return RGXOOM;
        }
        memcpy ( & rgx [len], m->rgx, l );
        len += l;
        break;
      case '"' :
        --len;
        quote = 1;
        break;
    }
  }

  if (!len) {
    error ("Did not encounter %%");
    return RGXERR;
  }

  error ("Incomplete rgx or missing action");
  return RGXERR;
}

/*
.. Naive way to read action. The action should be put inside a '{' '}'
.. block and the starting '{' should be placed in the same line of
.. the regex pattern
*/
static
int _action (char * buff, size_t lim) {

  int c, j = 0, depth = 0, quote = 0, escape = 0, comment = 0;
  #define PUSH(c)   do {                                             \
    if (c == '\n') ++line;                                           \
    buff [j++] = c;                                                  \
    if (j == lim) {                                                  \
      error ("action buff limit");                                   \
      return RGXERR;                                                 \
    }                                                                \
  } while (0)

  /*
  .. Consume white spaces before encountering '{' i.e start of action.
  .. Warning : Action need to be in the same line as regex.
  */
  while ( (c = fgetc(in)) != EOF ) {
    if ( c == '{' ) {
      depth++;
      buff [j++] = '{';
      break;
    }
    if ( !(c == ' ' || c == '\t' ) ) {
      error ("lxr grammar : Unexpected character"
        "between regex & action");
      return RGXERR;
    }
  }

  if (!depth) {
    error ("missing action");
    return RGXERR;
  }

  /*
  .. Doesn't validate C grammar that falls in action block { ... }.
  .. The whole action block will be stored inside 'buff' unless
  .. you run out of limit.
  */
  while ( depth && (c = fgetc(in)) != EOF ) {

    PUSH (c);

    if (quote) {
      if (escape)
        escape = 0;
      else if (c == '\\')
        escape = 1;
      else if (c == quote)
        quote = 0;
      continue;
    }

    if ( comment ) {
      comment  = 0;
      if ( c == '/' ) {
        /* consuming single line comment */
        while ( (c = fgetc(in)) != EOF ) {
          PUSH (c);
          if ( c == '\n' )
            break;
        }
        continue;
      }
      else if ( c == '*') {
        /* consuming multi line comment */
        while ( (c = fgetc(in)) != EOF ) {
          PUSH (c);
          if ( c != '*' ) continue;
          while ( (c = fgetc(in)) == '*' ) {
            PUSH (c);
          }
          PUSH (c);
          if ( c == '/' ) break;
        }
        continue;
      }
    }

    switch (c) {
      case '/' :
        comment = 1;
        break;
      case '"'  :
      case '\'' :
        quote = c;
        break;
      case '{' :
        depth++;
        break;
      case '}' :
        depth--;
        break;
    }
      
  }

  buff[j] = '\0';

  if (depth) {
    error ("Incomplete action. Missing '}'");
    return RGXERR;
  }

  /*
  .. Consume white spaces after the action '{' .. '}'
  */
  while ( (c = fgetc(in)) != EOF ) {
    if ( c == '\n') {
      line++;
      break;
    }
    if ( ! (c == ' ' || c == '\t' ) ) {
      error ("unexpected character after action");
      return RGXERR;
    }
  }
  return 0;
}

static inline 
void pattern_action ( const char * r, const char * a, int line) {
  Action * A = allocate (sizeof (Action));
  *A = (Action) { allocate_str (r), allocate_str (a), line };
  stack_push (actions, A);
}

int lex_read_rules ( ) {

  actions = stack_new (0);

  int status, cline;
  char rgx [RGXSIZE], action [PAGE_SIZE];

  for (;;) {
    cline = line;
    /* Read rgx pattern */
    status = _pattern (rgx, sizeof (rgx));
    if ( status == EOF ) break;
    if ( status == EMPTY ) continue;
    if ( status == RGXERR ) {
      error ("failed reading rule-action near line %d", line);
      return RGXERR;
    }
    if ( status == NOACTION ) {
      pattern_action (rgx, "", cline);
      continue;
    }
    #undef EMPTY
    #undef NOACTION

    /* Read the action for the above regex pattern */
    status = _action (action, sizeof (action));
    if ( status == RGXERR ) {
      error ("failed reading rule-action near line %d", line);
      return RGXERR;
    }

    pattern_action (rgx, action, cline);
  }
  return 0;  
}

/*
.. Create DFA from regex patterns, create the compressed tables for
.. lexical analysis and print the tables
*/
static int lex_print_tables () {

  int nrgx = (int) ( actions->len / sizeof (void *) );
  char ** rgx = allocate ( nrgx * sizeof (char *) );
  Action ** A = (Action **) actions->stack;
  for (int i=0; i<nrgx; ++i)
    rgx [i] = A[i]->rgx;
  
  DState * dfa = NULL;
  if (rgx_lexer_dfa (rgx, nrgx, &dfa) < 0) {       /* Minimised DFA */
    error ("failed to create a minimal dfa");
    return RGXERR;
  }

  int ** tables, * len;
  if (dfa_tables (&tables, &len) < 0) {  /* comprssd tbles from DFA */
    error ("Table size Out of memory limit");
    return RGXOOM;
  }

  /* if isdebug option is ON, print RPN of all regex */
  if (isdebug) {
    int rpn [RGXSIZE];
    for (int i=0; i<nrgx; ++i) {
      printf ("\nRpn for rgx \"%s\"", rgx[i]);
      rgx_rpn (rgx[i], rpn);
      rgx_rpn_print (rpn);
    }
  }

  char * names [] = {
    "check", "next", "base", "accept", "def", "meta", "class"
  };

  char * type [] = {
    "short", "short", "short", "short", "short",
    "unsigned char", "unsigned char"
  };

  int nclass = len [5], * class = tables [6];
  
  char buff [1024]; 
  sprintf ( buff, 
    "\n/*"
    "\n.. Equivalence classes for alphabets in [0x00, 0xFF] are"
    "\n.. stored in the table lxr_class []. The number of equivalence"
    "\n.. classes also counts the special equivalence classes like"
    "\n.. that of EOB/EOL/EOF/BOL. However BOL transition is never"
    "\n.. used, as the state 2 is reserved for the starting DFA in"
    "\n.. case of BOL status. lxr_eol_class will be set to 0 in case"
    "\n.. no patterns are found with EOL requirement."
    "\n*/"
    "\n#define lxr_nclass       %3d          /* num of eq classes */"
    "\n#define lxr_nel_class    %3d          /* new line  '\\n'    */"
    "\n#define lxr_eob_class    %3d          /* end of buffer     */"
    "\n#define lxr_eol_class    %3d          /* end of line       */"
    "\n#define lxr_eof_class    %3d          /* end of file       */",
    nclass, class ['\n'], EOB_CLASS,
    dfa_eol_used () ? EOL_CLASS : 0, EOF_CLASS );
  echo (buff);

  sprintf ( buff, 
    "\n\n/*"
    "\n.. States [1, %d] are reserved for tokens listed in the lex "
    "\n.. source file. In addition, the following two accept states"
    "\n.. are used internally for EOF/EOB"
    "\n*/"
    "\n#define lxr_eof_accept   %3d          /* end of file       */"
    "\n#define lxr_eob_accept   %3d          /* end of buffer     */",
    nrgx, nrgx + 1, nrgx + 2 );
  echo (buff);

  /*
  .. write all tables, before main lexer function
  */
  for (int i=0; i<7; ++i) {
    int * arr = tables [i], l = len [i];
    if (!arr) continue;
    sprintf ( buff, "\n\nstatic %s lxr_%s [%d] = {\n",
      type [i], names[i], l );
    echo (buff);
    for (int j=0; j<l; ++j) {
      sprintf ( buff, " %4d%s", arr[j], j == l-1 ? "" : ",");
      echo (buff);
      if (j%10 == 0)   echo ("\n");
      if (j%100 == 0)  echo ("\n");
    }
    echo ("\n};");
  }

  fflush (in);
  return 0;
}

static int lex_print_lxr_fnc () {

  char * before = stringify_tokenize, * end = before, c;
  while ( (c = *end) != '\0' )
  {
    if ( c == '/' && end [1] == '*' && end [2] == '%' )
    {
      *end = '\0';
      break;
    }
    end++;
  }
  assert (c != '\0');
  char * after = end + 1;
  while ( (c = *after++) != '\0' && c != '\n' ) {}
  assert (c != '\0');
  
  echo (before);
  size_t nrgx = ( actions->len / sizeof (void *) );
  Action ** A = (Action **) actions->stack;
  for (int i=0; i < nrgx; ++i) {
    fprintf ( out,
      "\n        case %d :"
      "\n          # line %d \"%s\""
      "\n          ", i+1, A[i]->line, infile);
    echo ( A[i]->action );
    fprintf ( out,
      "\n          break;\n");
    line_out += 5;
  }
  echo ("\n");
  echo_line (NULL, 0);
  echo (after);
  fflush (in);
  return 0;
}

static void lex_print_snippets () {

  if (snippet_head == NULL) return;

  int line = snippet_head->line;
  echo_line (infile, line);
  Snippet * s = snippet_head;

  while (s) {
    if (line++ != s->line) {
      line = s->line;
      echo_line (infile, line);
    }
    echo (s->code);
    s = s->next;
  }

  fflush (in);
}
  
int lex_print_source () {
  echo (stringify_api);
  echo (stringify_source);
  fflush (in);
  return 0;
}

int lex_print_last () {

  char buff [1024];
  size_t n;
  while ((n = fread(buff, 1, sizeof(buff)-1, in)) > 0) {
    buff [n] = '\0';
    echo (buff);
  }
  return 0;

}

int read_lex_input ( const char * _in_, const char * _out_ ) {

  infile = allocate_str (_in_ == NULL ? "<stdin>" : _in_);
  in = (_in_ != NULL) ? fopen ( infile, "r" ) : stdin;
  if (in == NULL) {
    error ("cannot find input %s", infile);
    return RGXERR;
  }

  outfile = allocate_str (_out_ == NULL ? "lxr.c" : _out_);
  out = fopen ( outfile, "w" );
  if (out == NULL) {
    error ("cannot write to %s", _out_);
    return RGXERR;
  }

  line = 0;
  /* write license first */
  echo (stringify_license);

  /* write the definition head in the %{ %} preamble of lex file */
  if (lex_read_definitions () < 0) {
    error ("failed reading definition section. line %d", line);
    return RGXERR;
  }
    
  line++;

  if (lex_read_rules () < 0) {
    error ("failed reading rules section. line %d", line);
    return RGXERR;
  }

  errors ();                  /* Flush any warnings to stderr*/

  lex_print_snippets ();

  echo_line (NULL, 0);

  if (lex_print_tables () < 0) {
    error ("failed creating tables");
    return RGXERR;
  }

  if (lex_print_source () < 0) {
    error ("lxr : failed writing lexer head");
    return RGXERR;
  }

  if (lex_print_lxr_fnc () < 0) {
    error ("failed writing lexer function");
    return RGXERR;
  }

  echo_line (infile, line+1);
  lex_print_last ();

  if (_in_ != NULL)
    fclose ( in );
  fclose ( out );

  return 0;
}
