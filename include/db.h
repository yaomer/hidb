#ifndef _HIDB_DB_H
#define _HIDB_DB_H

#include "sds.h"

struct __db;
typedef struct __db DB;

struct write_batch {
    sds_t buf;
};

/*
 * Using batch allows you to write multi modify operations at once,
 * which reduces the frequency of sync.
 */

/* Init a batch on the stack or you allocate memory for it */
void write_batch_init(struct write_batch *batch);
void write_batch_insert(struct write_batch *batch, struct slice *key, struct slice *value);
void write_batch_delete(struct write_batch *batch, struct slice *key);
void write_batch_clear(struct write_batch *batch);

/*
 * You just pass a reference to the key/value by slice,
 * you don't have to actually make a copy.
 */

DB     *db_open(const char *name);
/* Return 0 if ok, and save the result to *value
 * Return -1 if not found */
int     db_fetch(DB *db, struct slice *key, sds_t *value);
/* If insert the same key, and the original value is overwritten */
void    db_insert(DB *db, int sync, struct slice *key, struct slice *value);
/* If the key exists, delete it; else do nothing */
void    db_delete(DB *db, int sync, struct slice *key);
void    db_write(DB *db, int sync, struct write_batch *batch);
void    db_close(DB *db);

#endif /* _HIDB_DB_H */
