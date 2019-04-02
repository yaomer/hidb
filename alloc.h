#ifndef _REDB_ALLOC_H
#define _REDB_ALLOC_H

#include <sys/types.h>

void *db_malloc(size_t len);
void *db_calloc(size_t n, size_t len);
void db_free(void *);

#endif /* _REDB_ALLOC_H */
