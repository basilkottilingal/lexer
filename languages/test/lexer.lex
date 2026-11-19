%option yylineno
%%

aa|b       { return 1; }
a(a|b)     { return 2; }
bc+        { return 3; }
(a|b)+0    { return 4; }
abc$       { return 5; }
^abc       { return 6; }
abc        { return 7; }
abcd       { return 8; }

%%

int main () {
  int tkn;
  while ( (tkn = lxr_lex()) ) {
    printf ("\nl%d c%2d: [tkn : %d] %s",
      yylineno, yycolno, tkn, yytext);
  } 
}
