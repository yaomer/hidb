#ifndef _HIDB_ALLOC_H
#define _HIDB_ALLOC_H

#include <stddef.h>

void    *db_malloc(size_t size);
void    *db_calloc(size_t size);
void    *db_realloc(void *ptr, size_t size);
char    *db_strdup(const char *s);
void     db_free(void *ptr);

size_t  db_memory_used(void);

#endif /* _HIDB_ALLOC_H */
