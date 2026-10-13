

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
  lxr_dummy[3] = {lxr_eob_class, lxr_eob_class, lxr_eob_class};
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
  lxr_buff_stack * bf = malloc (sizeof (lxr_buff_stack));
  char * b = malloc (size + 2);
  if (lxr_class_buff_size < size)
    lxr_class_buff_size = size;
  lxr_class_buff =
    lxr_realloc (lxr_class_buff, lxr_class_buff_size + 2);
  if ( bf == NULL || b == NULL || lxr_class_buff == NULL ) {
    fprintf (stderr, "lxr_realloc failed");
    exit (-1);
  }
  bf->bytes = b;
  bf->next = lxr_buff_stack_current;
  lxr_buff_stack_current = bf;
  if (eob) b [0] = '\n';
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
  return (int) (unsigned char) 
   ( ((size_t) yyleng == idx) ? lxr_hold_char : yytext [idx] );
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
    return (int) (unsigned char) 
      ( ((size_t) yyleng == idx) ? lxr_hold_char : yytext [idx] );
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
      .bytes = lxr_alloc (size + 2),
      .next  = lxr_buff_stack_current
    };
    if (lxr_class_buff_size < size) {
      lxr_class_buff_size = size;
      lxr_class_buff = lxr_realloc (lxr_class_buff, size + 2); 
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
    s->bytes = lxr_realloc (s->bytes, size + 2);
    if (lxr_class_buff_size < size) {
      lxr_class_buff_size = size;
      lxr_class_buff = lxr_realloc (lxr_class_buff, size + 2); 
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
