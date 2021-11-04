#include "db.h"
#include "alloc.h"
#include "log.h"
#include "sds.h"
#include "concurrency_hash.h"
#include "error.h"

#include <string.h>
#include <assert.h>
#include <sys/stat.h>

DB *db_open(const char *name)
{
    DB *db = (DB*)db_malloc(sizeof(DB));
    size_t len = strlen(name);
    if (len == 0) db_err_sys("db name is empty");
    if (name[len - 1] != '/') {
        db->name = concat(name, "/");
    } else {
        db->name = concat(name, "");
    }
    mkdir(db->name, 0755);
    db->hs = concurrency_hash_init();
    db->log = log_init(db);
    db->query = sds_new();
    db->max_query_len = 0;
    return db;
}

sds_t *db_fetch(DB *db, const char *key, size_t keylen)
{
    struct value value = concurrency_hash_find(db->hs, key, keylen);
    if (value.size == -1) return NULL;
    if (db->max_query_len < value.size) {
        db->max_query_len = value.size;
        sds_resize(db->query, db->max_query_len);
    }
    db->query->len = value.size;
    sds_resize(db->query, value.size);
    load_value(db->query, &value);
    return db->query;
}

void db_insert(DB *db, int sync, const char *key, size_t keylen, const char *val, size_t vallen)
{
    struct value value;
    struct kvpair p;
    kvpair_init(p, key, keylen, val, vallen);
    log_append(db->log, &value, sync, Insert, &p);
    concurrency_hash_insert(db->hs, key, keylen, &value);
}

void db_delete(DB *db, int sync, const char *key, size_t keylen)
{
    struct kvpair p;
    kvpair_init(p, key, keylen, 0, 0);
    log_append(db->log, NULL, sync, Delete, &p);
    concurrency_hash_erase(db->hs, key, keylen);
}

void db_close(DB *db)
{
    log_dealloc(db->log);
    db_free(db->name);
    concurrency_hash_free(db->hs);
    sds_free(db->query);
    db_free(db);
}
