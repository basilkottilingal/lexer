#include "stack.h"
#include "allocator.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/*
.. fixme : recover old memory blocks, during realloc.
*/
static Stack * pool = NULL;
struct Freelist { Stack * next; };

void stack_dstr() {  /* Should be called when arena allocator is freed */
  pool = NULL;
}

static inline void stack_realloc ( Stack * s, int max ) {
  s->max = (max + 63) & ~(int) 63; /* nearest 64 Byte alignment. = 8 x (void *) */
  char * old = s->stack;           /* fixme : 'old' bytes are lost. */
  s->stack = allocate ( s->max );
  memcpy ( s->stack, old, s->len );
}

Stack * stack_new ( int len ) {
  len = len <= 0 ? 64 : len;
  Stack * s = pool;
  if (s && s->max >= len) {
    pool = ((struct Freelist *) s->stack)->next;
    stack_clear (s);
    return s;
  }
  s = allocate ( sizeof(Stack) );
  s->stack = allocate ( len );
  s->max = len;
  return s;
}

Stack * stack_copy ( Stack * p ) {
  Stack * s = stack_new ( p->len );
  s->nentries = p->nentries;
  s->flag = p->flag;
  s->len = p->len;
  memcpy (s->stack, p->stack, s->len);
  return s;
}

void stack_free ( Stack * s ) {
  ((struct Freelist *) s->stack)->next = pool;
  pool = s;
}

/*
static int ptrcmp (const void * a, const void * b) {
  return ((a>b) - (a<b));
}

void stack_sort ( Stack * s ) {
  qsort (s->stack, s->nentries, sizeof (void *), ptrcmp);
}

int stack_pcmp ( Stack * s1, Stack * s2 ){

  #define RGXCMP(p,q) cmp = ((int) (p > q) - (int) (p < q)); \
    if (cmp) return cmp

  int cmp;
  RGXCMP (s1->nentries, s2->nentries);
  void ** a = (void **) s1->stack, **b = (void **) s2->stack;
  for (int i=0; i<s1->nentries; ++i) {
    RGXCMP (a[i], b[i]);
  }
  return 0;

  #undef RGXCMP
}
*/

int stack_cmp ( Stack * s1, Stack * s2 ) {
  if ( s1->nentries != s2->nentries )
    return 1;
  if ( s1->len != s2->len )
    return 1;
  /*
  .. len is in bytes and need not be a multiple of 8 (see stack_bit),
  .. so compare all the 'len' bytes, including the trailing bytes
  */
  return memcmp (s1->stack, s2->stack, s1->len) != 0;
}

/*
.. Sets i-th bit to 1
.. Warning : n should be < max
*/
void stack_bit ( Stack * s, int i ) {
  int n = i >> 3;                     /* n-th byte */
  uint8_t * bits = (uint8_t *) s->stack, bit = ((uint8_t) 1) << (i&7);
  if (! (bits[n] & bit) ) s->nentries++;
  bits [n] |= bit;
  s->len = s->len > ++n ? s->len : n; /* array length = max(used byte) + 1 */
}

/*
.. Stack can also be used to push items/pop items.
.. data has to be converted to "void *". So max size/item = 8 Bytes
*/
void stack_push ( Stack * s, void * m ) {
  while (s->len + sizeof (void *) > s->max) {
    stack_realloc (s, 2 * s->max); /* double the size */
  }
  memcpy ( s->stack + s->len, &m, sizeof (void *) );
  s->nentries++;
  s->len += sizeof (void *);
}

/*
.. Murmur3 hashing
.. Warning : "max" has to be same for all stacks
*/
uint32_t stack_hash  ( uint32_t * key, int len ) {

  uint32_t k, h = 0;              /* Seed value is taken as 0 */
    for (int i = len >> 2; i; --i, key++) {
    k = *key;
    /*
    .. Scrambling bits of each 32 bit block
    */
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;

    h ^= k;
    h = ((h << 13) | (h >> 19)) * 5 + 0xe6546b64;
  }
  /*
  .. We skip the snippet of murmurhash algorith for
  .. trailing byte, which we know is absent in this case
  */
  h ^= len;
  h ^= (h >> 16);
  h *= 0x85ebca6b;
  h ^= (h >> 13);
  h *= 0xc2b2ae35;
  h ^= (h >> 16);
  return h;
}

void stack_reset ( Stack *  s ) {
  s->len = s->nentries = s->flag = 0;
}

void stack_clear ( Stack * s ) {
  s->len = s->nentries = s->flag = 0;
  memset (s->stack, 0, s->max);
}
