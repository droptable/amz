#ifndef _HAS_MEM_H
#define _HAS_MEM_H

void __attribute__((malloc)) *amz_alloc(size_t);
void __attribute__((malloc)) *amz_realloc(void*, size_t);
void __attribute__((malloc)) *amz_calloc(size_t, size_t);
void amz_free(void*);

#endif
