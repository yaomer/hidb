#ifndef _REDB_HASH_H
#define _REDB_HASH_H

#include "sds.h"

struct log_segment;

struct value {
    struct log_segment *seg; /* The log segment that holds the value */
    off_t   off;    /* The segment file offset */
    size_t  size;   /* value size */
};

struct hash_node {
    sds_t key;
    struct value value;
    struct hash_node *next;
};

typedef struct __hash {
    size_t hashnums;
    struct hash_node **buckets;
    size_t bucket_size;
} hash_t;

hash_t *hash_init(void);
struct value *hash_find(hash_t *hs, const char *key, size_t keylen);
/* If the key already exists and the value is updated,
 * the original value is overwritten */
void    hash_insert(hash_t *hs, const char *key, size_t keylen, struct value *value);
void    hash_erase(hash_t *hs, const char *key, size_t keylen);
/* Do not make changes while traversing */
void    hash_for_each(hash_t *hs, void (*deal)(void *arg, sds_t *key, struct value *value), void *arg);
void    hash_free(hash_t *hs);

#endif // _REDB_HASH_H
