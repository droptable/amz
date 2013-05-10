#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "mem.h"

#define AMZ_CMERROR(m, s) \
  if (!(m)) { \
    fputs(s, stderr); \
    exit(1); \
  }

void *amz_alloc(size_t s)
{
  void *m = malloc(s);
  AMZ_CMERROR(m, "memory error @ amz_alloc\n")
  return m;
}

void *amz_realloc(void *m, size_t s)
{
  void *n = realloc(m, s);
  AMZ_CMERROR(n, "memory error @ amz_realloc\n")
  return n;
}

void *amz_calloc(size_t s, size_t l)
{
  void *m = calloc(s, l);
  AMZ_CMERROR(m, "memory error @ amz_calloc\n")
  return m;
}

void amz_free(void *m) 
{
  if (m != NULL) free(m);
}
