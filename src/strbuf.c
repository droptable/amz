#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "strbuf.h"
#include "mem.h"

#define STRBUF_CHUNK 255

static inline void
extend(struct strbuf *buf, size_t min)
{
  size_t len = 0;
  
  while (len <= min) 
    len += STRBUF_CHUNK;
  
  // printf("strbuf.c: realloc %zu bytes\n", len);
  buf->avail = len;
  buf->buf = amz_realloc(buf->buf, len + buf->size);
}

struct strbuf *
strbuf_init(void)
{
  struct strbuf *buf;
  
  buf = amz_alloc(sizeof(struct strbuf));
  buf->size = 0;
  buf->avail = 0;
  buf->buf = NULL;
  
  return buf;
}

void 
strbuf_add(struct strbuf *buf, const char *str)
{
  strbuf_addn(buf, str, strlen(str));
}

void 
strbuf_addc(struct strbuf *buf, char c)
{
  strbuf_addn(buf, (char[]) { c }, 1);
}

void
strbuf_addn(struct strbuf *buf, const char *str, size_t len)
{
  // printf("strbuf.c: adding %zu bytes (avail = %zu)\n", len, buf->avail);
  
  if (buf->avail < len)
    extend(buf, len);
  
  assert(buf->avail >= len);
  
  size_t o = buf->size;
  size_t i = 0;
  
  while (i < len) buf->buf[o++] = str[i++];
  buf->size += len;
  buf->avail -= len;
}

char *
strbuf_cstr(struct strbuf *buf)
{
  char *str = amz_calloc(1, buf->size + 1);
  strncpy(str, buf->buf, buf->size);
  return str;
}

void 
strbuf_free(struct strbuf *buf)
{
  if (buf->buf != NULL) 
    amz_free(buf->buf);
  
  amz_free(buf);
}

void 
strbuf_clear(struct strbuf *buf)
{
  if (buf->buf != NULL) 
    amz_free(buf->buf);
  
  buf->size = 0;
  buf->avail = 0;
  buf->buf = NULL;
}
