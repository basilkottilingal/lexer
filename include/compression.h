#ifndef _ROW_CMP_H
#define _ROW_CMP_H

  #include <stdint.h>

  typedef struct Row {
    int s, n, token;
    uint32_t hash;
    int * stack;
  } Row;
  int rows_compression (Row **, int ***, int **, int n, int m);

  /*
  .. Optional meta equivalence classes (templates). rows_meta (1)
  .. before compression. After compression, rows_meta_state () is the
  .. state id of the first template (0 if no template was created).
  */
  void rows_meta ( int on );
  int  rows_meta_state ( );
  int  rows_ntemplates ( );
  int  rows_nmeta ( );

#endif
