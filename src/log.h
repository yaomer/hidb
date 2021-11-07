#ifndef _HIDB_LOG_H
#define _HIDB_LOG_H

#include <pthread.h>
#include <stdatomic.h>

#include "file.h"

typedef long long segno_t;

struct __db;
typedef struct __db DB;

struct slice;
struct value;
struct write_batch;

enum log_op_type {
    Insert = 1,
    Delete = 2,
};

/*
 * The log segment file is only appending and not modified.
 * Thus, when the background compact is in place, we can still
 * read segment files in compact concurrently.
 *
 * The segno of log segment files is strictly incremented.
 * The larger the segno is, the newer the segfile is.
 * e.g. log0, log1, ..., log10
 */
struct log_segment {
    segno_t segno;
    int     fd; /* Used to read the segment file */
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
    atomic_int  segments;   /* log segment nums */
    atomic_flag compacting; /* The background is compacting ? */
    /* 1) Sync append logs
     * 2) Ensure that the lsn corresponds to the actual location written to the file
     */
    pthread_mutex_t write_mtx;
    struct writable_file *curfile;
    off_t lsn; /* write offset of curfile */
} log_t;

log_t  *log_init(DB *db);
void    log_dealloc(log_t *log);
void    log_append(log_t *log, struct value *value, int sync, char type,
                   struct slice *key, struct slice *val);
void    load_value(sds_t *query, struct value *value);

void    write_batch_iterate(log_t *log, int sync, struct write_batch *batch);

char   *concat(const char *s1, const char *s2);

#endif /* _HIDB_LOG_H */
