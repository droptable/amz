#ifndef _HAS_AMZ_H
#define _HAS_AMZ_H

#include <stddef.h>

#define AMZAPI

struct amzinfo {
  char *name;
  char *value;
};

struct amzres {
  char *url;
  char *cover_src;
  char *cover_rel;
  char *title;
  char *desc;  
  struct {
    size_t size;
    struct amzinfo **items;
  } info;
};

AMZAPI struct amzres *amz_search(const char*);
AMZAPI struct amzres *amz_fetch(const char*);
AMZAPI void amz_clear(struct amzres*);

/* _HAS_AMZ_H */
#endif
