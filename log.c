#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <assert.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/stat.h>

#include "alloc.h"
#include "redb.h"
#include "log.h"
#include "concurrency_hash.h"
#include "error.h"

static const off_t max_segfile_size = 1024 * 1024 * 16;

static const int compact_segfile_nums = 4;

static off_t get_file_size(int fd)
{
    struct stat st;
    fstat(fd, &st);
    return st.st_size;
}

static char *get_segfile(log_t *log, segno_t segno)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "log%lld", segno);
    return concat(log->db->name, buf);
}

static void add_new_segment(log_t *log, segno_t segno)
{
    struct log_segment *seg = db_malloc(sizeof(struct log_segment));
    log->cur = seg;
    seg->segno = segno;

    char *segfile = get_segfile(log, segno);
    if (log->append_fd > 0) close(log->append_fd);
    log->append_fd = open(segfile, O_RDWR | O_APPEND | O_CREAT, 0644);
    if (log->append_fd < 0) db_err_sys("open(%s)", segfile);

    log->lsn = get_file_size(log->append_fd);
    seg->fd = open(segfile, O_RDONLY);
    if (seg->fd < 0) db_err_sys("open(%s)", segfile);
    db_free(segfile);

    /* Add seg to the list */
    pthread_mutex_lock(&log->base_mtx);
    seg->next = &log->base;
    seg->prev = log->base.prev;
    seg->prev->next = seg;
    seg->next->prev = seg;
    log->segments++;
    pthread_mutex_unlock(&log->base_mtx);
}

static void del_segment(log_t *log, struct log_segment *seg)
{
    pthread_mutex_lock(&log->base_mtx);
    seg->prev->next = seg->next;
    seg->next->prev = seg->prev;
    log->segments--;
    pthread_mutex_unlock(&log->base_mtx);
    close(seg->fd);
    db_free(seg);
}

static void replace_segment(log_t *log, struct log_segment *seg, struct log_segment *new_seg)
{
    pthread_mutex_lock(&log->base_mtx);
    new_seg->next = seg->next;
    new_seg->prev = seg->prev;
    new_seg->prev->next = new_seg;
    new_seg->next->prev = new_seg;
    pthread_mutex_unlock(&log->base_mtx);
    close(seg->fd);
    db_free(seg);
}

static int segno_compare(const void *arg1, const void *arg2)
{
    return *(segno_t*)arg1 - *(segno_t*)arg2;
}

static void build_segment_list(log_t *log)
{
    size_t mlen = 2;
    segno_t *segnos = db_malloc(mlen * sizeof(segno_t));
    size_t cur = 0;
    DIR *dirp = opendir(log->db->name);
    struct dirent *dp;
    while ((dp = readdir(dirp))) {
        if (strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, ".") == 0)
            continue;
        if (cur == mlen) {
            mlen *= 2;
            segnos = db_realloc(segnos, mlen * sizeof(segno_t));
        }
        segno_t segno = atoll(dp->d_name + 3);
        segnos[cur++] = segno;
    }
    closedir(dirp);

    /* We need to order the segment file by segno */
    qsort(segnos, cur, sizeof(segno_t), segno_compare);
    for (size_t i = 0; i < cur; i++) {
        add_new_segment(log, segnos[i]);
    }
    db_free(segnos);
}

/* Return parsed bytes and update *ptr */
static off_t parse_key(sds_t *key, char **ptr)
{
    key->len = *(size_t*)*ptr;
    *ptr += sizeof(key->len);
    key->buf = *ptr;
    *ptr += key->len;
    return sizeof(key->len) + key->len;
}

/* Return parsed bytes and update *ptr */
static off_t parse_value(struct value *value, char **ptr)
{
    value->size = *(size_t*)*ptr;
    *ptr += sizeof(value->size);
    *ptr += value->size;
    return sizeof(value->size) + value->size;
}

static void load_concurrency_hash_insert(void *hs, const char *key, size_t keylen, struct value *value)
{
    concurrency_hash_insert((concurrency_hash_t*)hs, key, keylen, value);
}

static void load_concurrency_hash_erase(void *hs, const char *key, size_t keylen)
{
    concurrency_hash_erase((concurrency_hash_t*)hs, key, keylen);
}

static void load_hash_insert(void *hs, const char *key, size_t keylen, struct value *value)
{
    hash_insert((hash_t*)hs, key, keylen, value);
}

static void load_hash_erase(void *hs, const char *key, size_t keylen)
{
    hash_erase((hash_t*)hs, key, keylen);
}

static void load_segment(struct log_segment *seg,
                         void (*insert)(void *hs, const char *key, size_t keylen, struct value *value),
                         void (*erase)(void *hs, const char *key, size_t keylen),
                         void *arg)
{
    sds_t key;
    struct value value;
    value.seg = seg;
    off_t lsn = 0;
    off_t file_size = get_file_size(seg->fd);
    void *start = mmap(NULL, file_size, PROT_READ, MAP_SHARED, seg->fd, 0);
    char *ptr = (char*)start;
    char *end = ptr + file_size;
    while (ptr < end) {
        char type = *ptr++;
        lsn++;
        lsn += parse_key(&key, &ptr);
        switch (type) {
        case Insert:
            lsn += parse_value(&value, &ptr);
            value.off = lsn - value.size;
            insert(arg, key.buf, key.len, &value);
            break;
        case Delete:
            erase(arg, key.buf, key.len);
            break;
        }
    }
    munmap(start, file_size);
}

static void replay(log_t *log)
{
    build_segment_list(log);
    struct log_segment *seg = log->base.next;
    for ( ; seg != &log->base; seg = seg->next) {
        load_segment(seg,
                     load_concurrency_hash_insert,
                     load_concurrency_hash_erase,
                     log->db->hs);
    }
}

struct compact_info {
    /* Compact range [begin, end] */
    struct log_segment *begin, *end;
    struct log_segment *tmp;
    off_t   lsn;
    sds_t  *buf;
    size_t  max_len;
    log_t  *log;
};

static off_t format_and_write(int fd, int type, struct kvpair *p)
{
    int iovlen;
    struct iovec iov[5];
    iov[0].iov_base = (char*)&type;
    iov[0].iov_len = 1;
    iov[1].iov_base = &p->key.len;
    iov[1].iov_len = sizeof(p->key.len);
    iov[2].iov_base = (char*)p->key.buf;
    iov[2].iov_len = p->key.len;
    if (type == Insert) {
        iov[3].iov_base = &p->value.len;
        iov[3].iov_len = sizeof(p->value.len);
        iov[4].iov_base = (char*)p->value.buf;
        iov[4].iov_len = p->value.len;
        iovlen = 5;
    } else {
        iovlen = 3;
    }
    if (writev(fd, iov, iovlen) < 0) {
        db_err_sys("writev");
    }
    size_t len = 1 + sizeof(p->key.len) + p->key.len;
    return type == Insert ? len + sizeof(p->value.len) + p->value.len : len;
}

static void do_compact(void *arg, sds_t *key, struct value *value)
{
    struct compact_info *info = (struct compact_info*)arg;
    if (info->max_len < value->size) {
        info->max_len = value->size;
        sds_resize(info->buf, info->max_len);
    }
    info->buf->len = value->size;
    load_value(info->buf, value);

    struct kvpair p;
    kvpair_init(p, key->buf, key->len, info->buf->buf, info->buf->len);
    info->lsn += format_and_write(info->tmp->fd, Insert, &p);
    value->seg = info->tmp;
    value->off = info->lsn - p.value.len;
    assert(value->size == p.value.len);
    concurrency_hash_insert(info->log->db->hs, key->buf, key->len, value);
}

static void *compact(void *arg)
{
    struct compact_info *info = (struct compact_info *)arg;

    char tmpfile[] = "tmp.XXXXX";
    mktemp(tmpfile);
    info->tmp->fd = open(tmpfile, O_RDWR | O_APPEND | O_CREAT, 0644);
    if (info->tmp->fd < 0) db_err_sys("open(%s)", tmpfile);

    /* Keep the latest versions of multiple same keys */
    hash_t *hs = hash_init();
    log_t *log = info->log;
    struct log_segment *seg = info->begin;
    while (seg != &log->base) {
        load_segment(seg, load_hash_insert, load_hash_erase, (void*)hs);
        if (seg == info->end) break;
        seg = seg->next;
    }

    /* Write the result of compact to a new segment file */
    hash_for_each(hs, do_compact, (void*)info);
    char *new_segfile = get_segfile(log, info->end->segno);
    info->tmp->segno = info->end->segno;
    rename(tmpfile, new_segfile);
    db_free(new_segfile);

    /* Delete all old segment files */
    seg = info->begin;
    while (seg != &log->base && seg != info->end) {
        char *old_segfile = get_segfile(log, seg->segno);
        del_segment(log, seg);
        unlink(old_segfile);
        db_free(old_segfile);
        seg = seg->next;
    }
    replace_segment(log, info->end, info->tmp);
    hash_free(hs);
    sds_free(info->buf);
    db_free(info);
    atomic_flag_clear(&log->compacting);
    return NULL;
}

static void maybe_compact(log_t *log)
{
    if (log->segments >= compact_segfile_nums && !atomic_flag_test_and_set(&log->compacting)) {
        struct compact_info *info = db_malloc(sizeof(struct compact_info));
        info->lsn = 0;
        info->log = log;
        info->begin = log->base.next;
        info->end = log->cur;
        info->tmp = db_malloc(sizeof(struct log_segment));
        info->buf = sds_new();
        pthread_t tid;
        pthread_create(&tid, NULL, compact, (void*)info);
    }
}

log_t *log_init(DB *db)
{
    log_t *log = (log_t*)db_malloc(sizeof(log_t));
    log->db = db;
    log->base.next = log->base.prev = &log->base;
    log->cur = NULL;
    log->segments = 0;
    log->append_fd = -1;
    replay(log);
    if (!log->cur) add_new_segment(log, 0);
    return log;
}

void log_dealloc(log_t *log)
{
    /* Wait for the background compact to complete */
    while (atomic_flag_test_and_set(&log->compacting)) ;
    struct log_segment *seg = log->base.next;
    while (seg != &log->base) {
        struct log_segment *tmp = seg->next;
        del_segment(log, seg);
        seg = tmp;
    }
    close(log->append_fd);
}

void log_append(log_t *log, struct value *value, int sync, int type, struct kvpair *p)
{
    log->lsn += format_and_write(log->append_fd, type, p);
    if (sync && fsync(log->append_fd) < 0) {
        db_err_sys("fsync(segno = %lld)", log->cur->segno);
    }
    if (value) {
        value->seg = log->cur;
        value->off = log->lsn - p->value.len;
        value->size = p->value.len;
    }
    if (log->lsn >= max_segfile_size) {
        maybe_compact(log);
        add_new_segment(log, log->cur->segno + 1);
    }
}

void load_value(sds_t *query, struct value *value)
{
    int fd = value->seg->fd;
    lseek(fd, value->off, SEEK_SET);
    if (read(fd, query->buf, value->size) < 0)
        db_err_sys("read(segno = %lld)", value->seg->segno);
}
