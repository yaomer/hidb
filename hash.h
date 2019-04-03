#ifndef _REDB_HASH_H
#define _REDB_HASH_H

#include <sys/types.h>

typedef struct redb_hash {
    void **h;   
    size_t hsize;
    size_t hnums;
} DB_HASH;

DB_HASH *db_hash_init(void);

#endif /* _REDB_HASH_H */
