#ifndef _LXR_H_
#define _LXR_H_

  /* 
  .. Available guaranteed buffersize.
  */
  #ifndef LXR_BUFFSIZE
  #define LXR_BUFFSIZE 256
  #endif
 
  int read_lex_input (const char *in, const char *out);

  void lxr_debug ();

  void lxr_meta_ecs ();       /* use meta equivalence classes ("-m") */

#endif
