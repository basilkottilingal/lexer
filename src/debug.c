#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "regex.h"

#define BOL_OPERAND    2048
#define EOL_OPERAND    4096
#define DOT_OPERAND    8192

void operand_string (int op, char * s)
{
  if (op == DOT_OPERAND)
  {
    sprintf (s, " .  "); return;
  }
  if (op == BOL_OPERAND)
  {
    sprintf (s, " ^  "); return;
  }
  if (op == EOL_OPERAND)
  {
    sprintf (s, " $  "); return;
  }
  if (op >= 512)
  {
    /* reduced operand */
    sprintf (s, "[%2d]", op - 512); return;
  }
  char c = (char) op;
  if (c == ' ' || (c >= 32 && c <= 126))
  {
    /* printable ascii range */
    sprintf (s, " '%c'", c); return;
  }
  /* non-printable ascii characters are printed as \xHH */
  sprintf (s, "\\x%02X", c);
}

void rgx_rpn_print_non_formatted ( int * rpn ) {
  char opStr [10];
  int op;
  printf ( "\n");
  while ( ( op = *rpn++ ) >= 0 ) {
    if (ISRGXOP (op))
      printf ( "\033[1;32m%c", (char) op);
    else
    {
      operand_string (op, opStr);
      printf ( "\033[1;31m%s", opStr);
    }
  }
}

int rgx_rpn_print ( int * rpn ) {

  #define  PUSH(_c_)  (buff[nbuff++] = _c_)
  #define  POP(_c_)   (nbuff ? buff[--nbuff] : RGXERR)

  rgx_rpn_print_non_formatted (rpn);

  int nbuff = 0;
  int buff [RGXSIZE];
  int inode = 512, e1, e2, op, c;
  char opStr [10];
  while ( ( op = *rpn++ ) >= 0 ) {
    if (ISRGXOP (op)) {

      if ( (c = (0xFF&op)) == '[' || c == '<' ) {
        printf ( "\n\t\033[1;32m[  ] =        %c", (char) c);
        continue; 
      }

      switch ( c ) {

        case 'd' : case 's' : case 'S' :
        case 'w' : case 'D' : case 'W' :
          assert (PUSH(op) >= 0); //not implemented
          break;
        case '.' :
          assert (PUSH(DOT_OPERAND) >= 0);
          break;
        case '^' :
          assert (PUSH(BOL_OPERAND) >= 0);
          break;
        case '$' :
          assert (PUSH(EOL_OPERAND) >= 0);
          break;

        case ',' : case ';' : case '-' : case '|' :
          e2 = POP(stack); e1 = POP(stack);
          assert(e1>=0 && e2>=0);
          printf ( "\n\t\033[1;32m[%2d] =", inode - 512);

          /* left operand */
          operand_string (e1, opStr);
          printf ("  \033[1;31m%s", opStr);

          /* operator */
          printf ("  \033[1;32m%c  ", (char) op);

          /* right operand */
          operand_string (e2, opStr);
          printf ("\033[1;31m%s", opStr);

          assert (PUSH(inode) >= 0); inode++;
          break;

        case 'q' :
          for (int i=0; i<4; ++i)
            assert (rpn[i] >= 0);
          e1 = POP(stack);
          assert(e1>=0);
          printf ( "\n\t\033[1;32m[%2d] =", inode - 512);

          /* quantified operand */
          operand_string (e1, opStr);
          printf ("  \033[1;31m%s", opStr);

          /* quantifier : {min, max} */
          printf ("\033[1;32m  {%d, %d}", rpn[1], rpn [2]);
          rpn += 4;

          assert (PUSH(inode) >= 0); inode++;
          break;

        case '*' : case ']' : case '+' :
        case '?' : case '>' :
          printf ( "\n\t\033[1;32m[%2d] =", inode - 512);
          e1 = POP(stack);
          assert(e1>=0);

          /* left (or the only) operand */
          operand_string (e1, opStr);
          printf ("  \033[1;31m%s", opStr);

          /* (unary) operator */
          printf ("  \033[1;32m%c", (char) op);
          assert (PUSH(inode) >= 0); inode++;
          break;

        default:
      }
    }
    else {
      assert (PUSH(op) >= 0);
    }
  }

  printf("\033[0m");
  fflush(stdout);
  return 0;

  #undef  PUSH
  #undef  POP
}
