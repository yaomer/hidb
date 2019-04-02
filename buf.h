#ifndef _REDB_BUF_H
#define _REDB_BUF_H

#include <sys/types.h>

typedef struct redb_buf {
    void *data;
    size_t len;
} DB_BUF;

#endif /* _REDB_BUF_H */
