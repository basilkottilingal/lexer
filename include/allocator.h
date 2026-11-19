#ifndef _H_ALLOCATE_
#define _H_ALLOCATE_
  /*
  .. A general pool allocator. Allocations up to PAGE_SIZE are served
  .. from large memory blocks; larger ones get a block of their own.
  .. Warning. There is no memory freeing (except at the end of pgm).
  .. So repeated allocations may exhaust RAM.
  */

  #include <stdlib.h>

  #ifndef BLOCK_SIZE
    #define BLOCK_SIZE (1<<16)    /* 64 kB     */
  #endif
  #ifndef PAGE_SIZE
    #define PAGE_SIZE 8192
  #endif

  /* APIs
  .. (a) "destroy" all mem blocks. Call @ the end of pgm
  .. (b) "allocate" memory. max 4088 Bytes. (Rounded to 8 bytes. Maybe unnecessary)
  .. (c) "allocate_str" : equivalent to strdup ()
  .. (d) "deallocate" a memory for reuse. size also should be passed correctly.
  .. (e) "reallocate" from an older size to a newer size. Copy old content also.
  */
  void   destroy ();
  void * allocate ( size_t size );
  char * allocate_str ( const char * s );
  void   deallocate ( void *, size_t );
  void * reallocate ( void *, size_t, size_t );
#endif
