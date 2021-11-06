#ifndef _HIDB_DB_IMPL_H
#define _HIDB_DB_IMPL_H

#include "log.h"
#include "concurrency_hash.h"

typedef struct __db {
    char    *name;  /* db dir name */
    log_t   *log;
    concurrency_hash_t  *hs;
} DB;

#endif /* _HIDB_DB_IMPL_H */
