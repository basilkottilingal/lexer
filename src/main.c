#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "regex.h"
#include "lexer.h"

void usages ( char * pgm ) {
  const char * usage [] = {
    "-o output.c lexer.l",
    "-m -o output.c lexer.l     (meta equivalence classes)",
    "-o output.c < lexer.l",
    "lexer.l",
    "< lexer.l"
  };
  fprintf (stderr, "\nusages(s):");
  for (int i=0; i<sizeof usage/ sizeof usage[0]; ++i)
    fprintf (stderr, "\n  %s %s", pgm, usage [i]);
}

int main (int argc, char ** argv) {

  char * in = NULL;
  char * out = NULL;

  for (int i=1; i<argc; ++i) {
    if (argv[i][0] == '-') {
      if (!strcmp (argv [i], "-o")) {
        if (argc == ++i) {
          fprintf (stderr, "\nmissing out put file");
          usages (argv[0]);
          exit (-1);
        }
        out = argv [i];
        continue;
      }
      if (!strcmp (argv [i], "-m")) {
        lxr_meta_ecs ();
        continue;
      }
      if (!strcmp (argv [i], "-d")) {
        lxr_debug ();
        continue;
      }
      fprintf (stderr, "\nunknown flag %s", argv[i]);
      usages (argv[0]);
      exit (-1);
    }
    if (in != NULL) {
      usages (argv[0]);
      exit(-1);
    }
    in = argv [i];
  }

  #if 0
  if (in == NULL) {
    fprintf (stdout, "\nwaiting for stdin..");
  }
  #endif

  if ( read_lex_input (in, out) ) {
    fprintf (stderr, "failed creating lexer code");
    errors ();
    exit (-1);
  }

  /*
  .. clean allocated memory blocks
  */
  rgx_free (); 

  return 0;
}
