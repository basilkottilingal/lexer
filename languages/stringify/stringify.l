%{
/*
.. convert all .c files into const string [] literals.
*/

#define YY_DECL   int yylex ()

#include <stdio.h>
#include <assert.h>
#include <string.h>

%}

%%
[^"\\\x00-\x1F]+ {printf ("%s", yytext); }
\x0a   { printf ("\\n\"\n\t\""); }
\x09   { printf ("\\t"); }
\x07   { printf ("\\a"); }
\x0b   { printf ("\\v"); }
\x08   { printf ("\\b"); }
\x0d   { printf ("\\r"); }
\x0c   { printf ("\\f"); }
\"     { printf ("\\\""); }
\\     { printf ("\\\\"); }
.      { assert (0 && "character not allowed in c string literals"); }

%%

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
