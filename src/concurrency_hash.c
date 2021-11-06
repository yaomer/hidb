#include "alloc.h"
#include "concurrency_hash.h"

static const size_t default_segment_size = 16;

static struct hash_segment *__segment(concurrency_hash_t *hs, struct slice *key)
{
    size_t idx = strhash(key->buf, key->len) & (hs->segment_size - 1);
    return hs->segments[idx];
}

concurrency_hash_t *concurrency_hash_init(void)
{
    concurrency_hash_t *hs = db_malloc(sizeof(concurrency_hash_t));
    hs->segment_size = default_segment_size;
    hs->segments = db_malloc(hs->segment_size * sizeof(struct hash_segment *));
    for (int i = 0; i < hs->segment_size; i++) {
        hs->segments[i] = db_malloc(sizeof(struct hash_segment));
        pthread_mutex_init(&hs->segments[i]->mtx, NULL);
        hs->segments[i]->hs = hash_init();
    }
    return hs;
}

struct value concurrency_hash_find(concurrency_hash_t *hs, struct slice *key)
{
    struct value rvalue;
    rvalue.seg = NULL; /* not found */
    struct hash_segment *seg = __segment(hs, key);
    pthread_mutex_lock(&seg->mtx);
    struct value *value = hash_find(seg->hs, key);
    if (value) rvalue = *value;
    pthread_mutex_unlock(&seg->mtx);
    return rvalue;
}

void concurrency_hash_insert(concurrency_hash_t *hs, struct slice *key, struct value *value)
{
    struct hash_segment *seg = __segment(hs, key);
    pthread_mutex_lock(&seg->mtx);
    hash_insert(seg->hs, key, value);
    pthread_mutex_unlock(&seg->mtx);
}

void concurrency_hash_erase(concurrency_hash_t *hs, struct slice *key)
{
    struct hash_segment *seg = __segment(hs, key);
    pthread_mutex_lock(&seg->mtx);
    hash_erase(seg->hs, key);
    pthread_mutex_unlock(&seg->mtx);
}

void concurrency_hash_free(concurrency_hash_t *hs)
{
    for (int i = 0; i < hs->segment_size; i++) {
        hash_free(hs->segments[i]->hs);
        pthread_mutex_destroy(&hs->segments[i]->mtx);
        db_free(hs->segments[i]);
    }
    db_free(hs->segments);
    db_free(hs);
}
