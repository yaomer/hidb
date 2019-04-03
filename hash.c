#include "alloc.h"
#include "hash.h"

#define DB_HASH_INIT_SIZE 16

DB_HASH *
db_hash_init(void)
{
    DB_HASH *dh = db_malloc(sizeof(DB_HASH));

    dh->h = db_calloc(DB_HASH_INIT_SIZE, sizeof(void *));
    dh->hsize = DB_HASH_INIT_SIZE;
    dh->hnums = 0;

    return dh;
}
