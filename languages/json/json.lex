%{

/*
.. standalone test 
.. gcc -DJSON_LEX_TESTING -o json-lxr <this.c> && ./json-lxr < test.json
*/

/* let's use 'yylex' for the tokenizer as bison only expects yylex */
#define YYSTYPE  int yylex ()

/*
.. refer to the combined lexer + grammar rules of json at
.. https://www.json.org/json-en.html 
.. https://www.crockford.com/mckeeman.html
*/
typedef enum
{
  /*
  .. don't use TOKEN ID < 256 as there is a chance it might
  .. clash with ascii characters
  */
  J_TRUE          = 257,
  J_FALSE,
  J_NULL,
  J_STRING,
  J_NUMBER,
} JSON_TOKEN;

%}

%%

true                                            { return J_TRUE; }
false                                           { return J_FALSE; }
null                                            { return J_NULL; }
\"([^"\\\x00-\x1F]|\\["\\/bfnrt]|\\u[0-9a-fA-F]{4})*\"   { return J_STRING; }
\[|\]|:|,|\{|\}                                 { return yytext [0]; }
-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?  { return J_NUMBER; }
[ \t\v\n\f]+                                    { /* skip */ }
.                                               { fprintf (stderr, "\nerror : %s", yytext); }

%%

#ifdef JSON_LEX_TESTING
int main (int argc, char ** argv)
{
  /* user may pass a filename as an additional arguement or use stdin */
  if (argc >= 2)
    lxr_source (argv [1]);
  /* by default it is stdin */

  while ( yylex () ) {
    printf ("\n%s", yytext);
  }
  lxr_clean (); 
}
#endif
