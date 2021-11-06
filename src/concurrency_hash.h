#ifndef _HIDB_CONCURRENCY_HASH_H
#define _HIDB_CONCURRENCY_HASH_H

#include <pthread.h>

#include "hash.h"

/* Mutex must be initialized before use!!! */
struct hash_segment {
    hash_t *hs;
    /* Consider using read-write lock */
    pthread_mutex_t mtx;
};

/* Simple segmental locking to reduce lock conflicts */
typedef struct __concurrency_hash {
    struct hash_segment **segments;
    size_t segment_size;
} concurrency_hash_t;

concurrency_hash_t *concurrency_hash_init(void);
struct value concurrency_hash_find(concurrency_hash_t *hs, struct slice *key);
void    concurrency_hash_insert(concurrency_hash_t *hs, struct slice *key, struct value *value);
void    concurrency_hash_erase(concurrency_hash_t *hs, struct slice *key);
void    concurrency_hash_free(concurrency_hash_t *hs);

#endif /* _HIDB_CONCURRENCY_HASH_H */
