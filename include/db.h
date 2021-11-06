#ifndef _HIDB_DB_H
#define _HIDB_DB_H

#include "sds.h"

struct __db;
typedef struct __db DB;

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
void    db_delete(DB *db, int sync, struct slice *key);
void    db_close(DB *db);


#endif /* _HIDB_DB_H */
