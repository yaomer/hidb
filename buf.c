#include <stdio.h>
#include <string.h>
#include "alloc.h"
#include "buf.h"

#define DB_BUF_INIT_SIZE 16

DB_BUF *
db_buf_init(void)
{
    DB_BUF *dbuf = db_malloc(sizeof(DB_BUF));

    dbuf->data = db_malloc(DB_BUF_INIT_SIZE);
    dbuf->max_len = DB_BUF_INIT_SIZE;

    return dbuf;
}

void
db_buf_update(DB_BUF *dbuf, size_t len)
{
    if (len > dbuf->max_len) {
        while (len > dbuf->max_len)
            dbuf->max_len *= 2;
        db_free(dbuf->data);
        dbuf->data = db_malloc(dbuf->max_len);
    }
}

void
db_buf_free(DB_BUF *dbuf)
{
    db_free(dbuf->data);
    db_free(dbuf);
}

DB_STR *
db_str_init(size_t len)
{
    DB_STR *dstr = db_malloc(sizeof(DB_STR));

    dstr->str = db_malloc(len);
    dstr->len = len;

    return dstr;
}
