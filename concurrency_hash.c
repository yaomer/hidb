#include "alloc.h"
#include "concurrency_hash.h"

static const int default_segment_size = 16;

static struct hash_segment *__segment(concurrency_hash_t *hs, const char *key, size_t keylen)
{
    sds_t sds;
    sds.buf = (char*)key;
    sds.len = keylen;
    return hs->segments[sds_hash(&sds) & (hs->segment_size - 1)];
}

concurrency_hash_t *concurrency_hash_init(void)
{
    concurrency_hash_t *hs = db_malloc(sizeof(concurrency_hash_t));
    hs->segment_size = default_segment_size;
    hs->segments = db_malloc(hs->segment_size * sizeof(struct hash_segment *));
    for (int i = 0; i < hs->segment_size; i++) {
        hs->segments[i] = db_malloc(sizeof(struct hash_segment));
        hs->segments[i]->hs = hash_init();
    }
    return hs;
}

struct value concurrency_hash_find(concurrency_hash_t *hs, const char *key, size_t keylen)
{
    struct value rvalue;
    rvalue.size = -1; /* not found */
    struct hash_segment *seg = __segment(hs, key, keylen);
    pthread_mutex_lock(&seg->mtx);
    struct value *value = hash_find(seg->hs, key, keylen);
    if (value) rvalue = *value;
    pthread_mutex_unlock(&seg->mtx);
    return rvalue;
}

void concurrency_hash_insert(concurrency_hash_t *hs, const char *key, size_t keylen, struct value *value)
{
    struct hash_segment *seg = __segment(hs, key, keylen);
    pthread_mutex_lock(&seg->mtx);
    hash_insert(seg->hs, key, keylen, value);
    pthread_mutex_unlock(&seg->mtx);
}

void concurrency_hash_erase(concurrency_hash_t *hs, const char *key, size_t keylen)
{
    struct hash_segment *seg = __segment(hs, key, keylen);
    pthread_mutex_lock(&seg->mtx);
    hash_erase(seg->hs, key, keylen);
    pthread_mutex_unlock(&seg->mtx);
}

void concurrency_hash_free(concurrency_hash_t *hs)
{
    for (int i = 0; i < hs->segment_size; i++) {
        hash_free(hs->segments[i]->hs);
        db_free(hs->segments[i]);
    }
    db_free(hs->segments);
    db_free(hs);
}
