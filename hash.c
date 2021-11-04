#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "alloc.h"
#include "sds.h"
#include "hash.h"
#include "log.h"

static const int hash_init_bucket_size = 16;

static const double hash_max_load_factor = 0.75;

static const double hash_min_load_factor = 0.25;

static const int hash_incr_factor = 2;

#define __check_hash(hash) (assert(hash))

static struct hash_node **__alloc_buckets(size_t bucket_size)
{
    struct hash_node **buckets = db_calloc(bucket_size * sizeof(struct hash_node *));
    return buckets;
}

static void __free_buckets(struct hash_node **buckets)
{
    db_free(buckets);
}

static struct hash_node *__alloc_node(const char *key, size_t keylen, struct value *value)
{
    struct hash_node *node = db_malloc(sizeof(struct hash_node));
    sds_init(&node->key);
    sds_append2(&node->key, key, keylen);
    node->value = *value;
    node->next = NULL;
    return node;
}

static void __free_node(struct hash_node *node)
{
    sds_clear(&node->key);
    db_free(node);
}

static size_t __hash(hash_t *hs, sds_t *key)
{
    return sds_hash(key) & (hs->bucket_size - 1);
}

#define __for_each(buckets, bucket_size, deal) \
    do { \
        for (size_t i = 0; i < (bucket_size); i++) { \
            struct hash_node *node = buckets[i]; \
            while (node) { \
                struct hash_node *tmp = node->next; \
                deal; \
                node = tmp; \
            } \
        } \
    } while (0)

static struct hash_node *__hash_find(hash_t *hs, const char *key, size_t keylen)
{
    sds_t sdskey;
    sdskey.buf = (char*)key;
    sdskey.len = keylen;
    size_t hashval = __hash(hs, &sdskey);
    struct hash_node *node = hs->buckets[hashval];
    for ( ; node; node = node->next)
        if (sds_cmp(&node->key, &sdskey) == 0)
            return node;
    return NULL;
}

static void __hash_insert(hash_t *hs, struct hash_node *node)
{
    size_t hashval = __hash(hs, &node->key);
    node->next = hs->buckets[hashval];
    hs->buckets[hashval] = node;
    hs->hashnums++;
}

static double __hash_load_factor(hash_t *hs)
{
    return (hs->hashnums * 1.0) / hs->bucket_size;
}

static void __hash_expand_or_shrink(hash_t *hs, int expand)
{
    struct hash_node **old_buckets = hs->buckets;
    size_t old_bucket_size = hs->bucket_size;
    if (expand) {
        hs->bucket_size *= hash_incr_factor;
    } else {
        hs->bucket_size /= hash_incr_factor;
    }
    hs->buckets = __alloc_buckets(hs->bucket_size);
    hs->hashnums = 0;

    __for_each(old_buckets, old_bucket_size, __hash_insert(hs, node));
    __free_buckets(old_buckets);
}

static void __hash_expand(hash_t *hs)
{
    if (__hash_load_factor(hs) >= hash_max_load_factor)
        __hash_expand_or_shrink(hs, 1);
}

static void __hash_shrink(hash_t *hs)
{
    if (__hash_load_factor(hs) <= hash_min_load_factor && hs->bucket_size > hash_init_bucket_size)
        __hash_expand_or_shrink(hs, 0);
}

hash_t *hash_init(void)
{
    hash_t *hs = db_malloc(sizeof(hash_t));
    hs->bucket_size = hash_init_bucket_size;
    hs->buckets = __alloc_buckets(hs->bucket_size);
    hs->hashnums = 0;
    return hs;
}

struct value *hash_find(hash_t *hs, const char *key, size_t keylen)
{
    __check_hash(hs);
    struct hash_node *node = __hash_find(hs, key, keylen);
    return node ? &node->value : NULL;
}

void hash_insert(hash_t *hs, const char *key, size_t keylen, struct value *value)
{
    __check_hash(hs);
    struct hash_node *node = __hash_find(hs, key, keylen);
    if (node) {
        if (node->value.seg->segno <= value->seg->segno) {
            node->value = *value;
        }
        return;
    }
    node = __alloc_node(key, keylen, value);
    __hash_insert(hs, node);
    __hash_expand(hs);
}

void hash_erase(hash_t *hs, const char *key, size_t keylen)
{
    __check_hash(hs);
    sds_t sdskey;
    sdskey.buf = (char*)key;
    sdskey.len = keylen;
    size_t hashval = __hash(hs, &sdskey);
    struct hash_node *node = hs->buckets[hashval];
    struct hash_node *pre = NULL;

    for ( ; node; node = node->next) {
        if (sds_cmp(&node->key, &sdskey) == 0)
            break;
        pre = node;
    }
    if (!node) return;
    if (pre)
        pre->next = node->next;
    else
        hs->buckets[hashval] = node->next;
    __free_node(node);
    hs->hashnums--;
    __hash_shrink(hs);
}

void hash_for_each(hash_t *hs, void (*deal)(void *arg, sds_t *key, struct value *value), void *arg)
{
    __for_each(hs->buckets, hs->bucket_size, deal(arg, &node->key, &node->value));
}

void hash_free(hash_t *hs)
{
    __for_each(hs->buckets, hs->bucket_size, __free_node(node));
    __free_buckets(hs->buckets);
    db_free(hs);
}
