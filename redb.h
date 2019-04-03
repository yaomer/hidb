#ifndef _REDB_H
#define _REDB_H

#include <fcntl.h>
#include "hash.h"
#include "buf.h"

/*
 * [free ptr] [hashsize] [hashnums] [hash list ...]
 * [nrec ptr] [idxlen] [key]:[datoff]:[datlen]
 */
typedef struct redb {
    char *name;
    DB_HASH *hash;
    DB_BUF *buf;

    int idx_fd;
    size_t idx_len;
    off_t idx_off;
    int dat_fd;
    size_t dat_len;
    off_t dat_off;

    off_t nrec_ptr;  /* 指向下一个索引记录的指针(偏移) */
    off_t chain_off;
    off_t hash_off;
} DB;

/* [nrec ptr] and [idxlen] and [hash list]*/
#define DB_FIELD_SZ 8
#define DB_FREE_OFF 0

#define DB_OPEN_MODE (O_RDWR | O_CREAT)
#define DB_FILE_MODE 0660

#define SEP ':'

enum DB_STORE_TYPE {
    DB_INSERT,
    DB_REPLACE,
    DB_STORE,
};

#endif /* _REDB_H */
