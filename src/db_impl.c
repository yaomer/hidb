#include <string.h>
#include <assert.h>
#include <sys/stat.h>

#include "db_impl.h"
#include "alloc.h"
#include "error.h"

DB *db_open(const char *name)
{
    DB *db = (DB*)db_malloc(sizeof(DB));
    size_t len = strlen(name);
    if (len == 0) db_error("db name is empty");
    if (name[len - 1] != '/') {
        db->name = concat(name, "/");
    } else {
        db->name = db_strdup(name);
    }
    mkdir(db->name, 0755);
    db->hs = concurrency_hash_init();
    db->log = log_init(db);
    return db;
}

int db_fetch(DB *db, struct slice *key, sds_t *value)
{
    struct value val = concurrency_hash_find(db->hs, key);
    if (!val.seg) return -1;
    load_value(value, &val);
    return 0;
}

void db_insert(DB *db, int sync, struct slice *key, struct slice *value)
{
    struct value val;
    log_append(db->log, &val, sync, Insert, key, value);
    concurrency_hash_insert(db->hs, key, &val);
}

void db_delete(DB *db, int sync, struct slice *key)
{
    log_append(db->log, NULL, sync, Delete, key, NULL);
    concurrency_hash_erase(db->hs, key);
}

void db_write(DB *db, int sync, struct write_batch *batch)
{
    write_batch_iterate(db->log, sync, batch);
    sds_resize(&batch->buf, 0);
}

void db_close(DB *db)
{
    log_dealloc(db->log);
    db_free(db->name);
    concurrency_hash_free(db->hs);
    db_free(db);
}
