#ifndef _HAS_STRBUF_H
#define _HAS_STRBUF_H

#include <stddef.h>
#include <stdbool.h>

struct strbuf {
  size_t size; /* current size */
  size_t avail; /* available space (before a realloc is required) */
  char *buf;
};

struct strbuf *strbuf_init(void);
void strbuf_add(struct strbuf*, const char*);
void strbuf_addc(struct strbuf *, char);
void strbuf_addn(struct strbuf*, const char*, size_t);
char *strbuf_cstr(struct strbuf*);
void strbuf_free(struct strbuf*);
void strbuf_clear(struct strbuf*);

#endif
