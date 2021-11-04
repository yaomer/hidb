#ifndef _HIDB_LOG_H
#define _HIDB_LOG_H

#include <pthread.h>
#include <stdatomic.h>

#include "sds.h"

typedef long long segno_t;

struct __db;
typedef struct __db DB;
struct value;

enum log_op_type {
    Insert = 1,
    Delete = 2,
};

struct kvpair {
    sds_t key, value;
};

#define kvpair_init(p, __key, __keylen, __val, __vallen) \
    do { \
        sds_init(&(p).key); \
        sds_init(&(p).value); \
        (p).key.buf = (char*)(__key); \
        (p).key.len = (__keylen); \
        (p).value.buf = (char*)(__val); \
        (p).value.len = (__vallen); \
    } while (0)

/*
 * The log segment file is only appending and not modified.
 * Thus, when the background compact is in place, we can still
 * read segment files in compact concurrently.
 */
struct log_segment {
    /* The segno of log segment files is strictly incremented.
     * The larger the segno is, the newer the segfile is. */
    segno_t segno;
    int     fd; /* Open segment file */
    struct log_segment *prev;
    struct log_segment *next;
};

typedef struct __log {
    DB  *db;
    pthread_mutex_t base_mtx;
    /* All log segments organized as a sorted double-linked list by segno */
    struct log_segment base;
    /* Point to the log segment currently being used for writing */
    struct log_segment *cur;
    atomic_int  segments;   /* Log segment nums */
    atomic_flag compacting; /* The background is compacting ? */
    int         append_fd;  /* Used for appending write */
    off_t       lsn;        /* The offset of the current writing position */
} log_t;

log_t  *log_init(DB *db);
void    log_dealloc(log_t *log);
/* It's not thread-safe yet */
void    log_append(log_t *log, struct value *value, int sync, int type, struct kvpair *p);
void    load_value(sds_t *query, struct value *value);

#endif /* _HIDB_LOG_H */
