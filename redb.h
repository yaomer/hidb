#ifndef _REDB_H
#define _REDB_H

#include "sds.h"

struct __concurrency_hash;
typedef struct __concurrency_hash concurrency_hash_t;
struct __log;
typedef struct __log log_t;

typedef struct redb {
    char    *name;  /* db dir name */
    log_t   *log;   /* log module */
    sds_t   *query; /* Saves the results of user queries */
    size_t   max_query_len;
    concurrency_hash_t  *hs;
} DB;

DB     *db_open(const char *name);
sds_t  *db_fetch(DB *db, const char *key, size_t keylen);
void    db_insert(DB *db, int sync, const char *key, size_t keylen, const char *val, size_t vallen);
void    db_delete(DB *db, int sync, const char *key, size_t keylen);
void    db_close(DB *db);

#endif /* _REDB_H */
