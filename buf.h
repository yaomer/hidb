#ifndef _REDB_BUF_H
#define _REDB_BUF_H

#include <sys/types.h>

typedef struct redb_buf {
    void *data;
    size_t max_len;
} DB_BUF;

/* 返回给客户的数据 */
typedef struct redb_str {
    char *str;
    size_t len;
} DB_STR;

DB_BUF *db_buf_init(void);
void db_buf_update(DB_BUF *dbuf, size_t len);
DB_STR *db_str_init(size_t len);

#endif /* _REDB_BUF_H */
