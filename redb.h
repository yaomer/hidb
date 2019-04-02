#ifndef _REDB_H
#define _REDB_H

#include <sys/types.h>
#include "hash.h"
#include "buf.h"

/*
 * [nrec ptr] [idxlen] [key]:[datoff]:[datlen]
 */
typedef struct redb {
    char *name;
    DB_HASH *hash;

    int idx_fd;
    DB_BUF *idx_buf; 
    size_t idx_len;
    off_t idx_off;

    int dat_fd;
    DB_BUF *dat_buf;
    size_t dat_len;
    off_t dat_off;

    off_t nrec_ptr;
} DB;

#endif /* _REDB_H */
