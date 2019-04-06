#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include "redb.h"
#include "alloc.h"
#include "lock.h"
#include "buf.h"
#include "error.h"

/*
 * 为DB结构分配内存，并初始化相应字段
 */
static DB *
_db_init(size_t len)
{
    DB *db = db_calloc(1, sizeof(DB));

    /* 后缀名(4) + \n */
    db->name = db_malloc(len + 4 + 1);
    db->buf = db_buf_init();
    db->idxfd = db->datfd = -1;
    db->freeoff = DB_FREE_OFF;
    db->hashsize = DB_HASH_INIT_SIZE;
    db->hashnums = 0;
    db->hashoff = DB_HASH_OFF;
    db->lock = DB_UNLOCK;

    return db;
}

static void
_db_free(DB *db)
{
    db_free(db->name);
    db_buf_free(db->buf);
    db_free(db);
}

/*
 * 初始化索引文件，即填充下列字段：
 * [free list off] [hashsize] [hashnums] [hash list] [\n]
 */
static void
_db_idxf_init(DB *db)
{
    char idx[DB_FIELD_SZ * (3 + DB_HASH_INIT_SIZE) + 3];
    char hlist[DB_FIELD_SZ + 1];
    size_t len;

    snprintf(idx, sizeof(idx), "%*lld%*zu%*zu",
            DB_FIELD_SZ, db->freeoff, DB_FIELD_SZ, db->hashsize,
            DB_FIELD_SZ, db->hashnums);
    snprintf(hlist, sizeof(hlist), "%*lld", DB_FIELD_SZ, 0LL);
    for (int i = 0; i < db->hashsize; i++)
        strcat(idx, hlist);
    strcat(idx, "\n");

    len = strlen(idx);
    if (write(db->idxfd, idx, len) != len)
        db_err_sys("_db_idxf_init: write error");
}

/*
 * 读取索引文件内容并以之初始化DB
 */
static void
_db_read_idxf_init(DB *db)
{

}

DB *
db_open(const char *name)
{
    size_t len = strlen(name);
    DB *db = _db_init(len);
    struct stat statbuf;

    strcpy(db->name, name);
    strcat(db->name, ".idx");
    if ( (db->idxfd = open(db->name, DB_OPEN_MODE, DB_FILE_MODE)) < 0)
        db_err_sys("db_open: open error");
    strcpy(db->name + len, ".dat");
    if ( (db->datfd = open(db->name, DB_OPEN_MODE, DB_FILE_MODE)) < 0)
        db_err_sys("db_open: open error");

    if (db->lock && db_writelock(db->idxfd, 0, SEEK_SET, 0) < 0)
        db_err_quit("db_open: db_writelock error");

    if (fstat(db->idxfd, &statbuf) < 0)
        db_err_sys("db_open: fstat error");

    if (statbuf.st_size == 0)
        _db_idxf_init(db);
    else
        _db_read_idxf_init(db);

    if (db->lock && db_unlock(db->idxfd, 0, SEEK_SET, 0) < 0)
        db_err_quit("db_open: db_unlock error");
    return db;
}

void
db_close(DB *db)
{
    _db_free(db);
}

static unsigned
_db_hash(DB *db, const char *key)
{
    unsigned hval;

    for (hval = 0; *key != '\0'; key++)
        hval = *key + 31 * hval;
    return hval % db->hashsize;
}

/*
 * 读取一条索引记录，并填充
 * [db->nrecoff] [db->idxlen] [db->datoff] [db->datlen]
 */
static void
_db_read_idx(DB *db)
{
    struct iovec iov[2];
    char nrecoff[DB_FIELD_SZ + 1];
    char idxlen[DB_FIELD_SZ + 1];
    char *datoff, *datlen;

    if (lseek(db->idxfd, db->nrecoff, SEEK_SET) < 0)
        db_err_sys("_db_read_idx: lseek error");

    iov[0].iov_base = nrecoff;
    iov[0].iov_len = DB_FIELD_SZ;
    iov[1].iov_base = idxlen;
    iov[1].iov_len = DB_FIELD_SZ;

    if (readv(db->idxfd, iov, 2) < 0)
        db_err_sys("_db_read_idx: readv error");

    db->idxlen = atol(idxlen);

    if (lseek(db->idxfd, db->nrecoff + DB_FIELD_SZ * 2 + 1, SEEK_SET) < 0)
        db_err_sys("_db_read_idx: lseek error");

    db->nrecoff = atol(nrecoff);

    db_buf_update(db->buf, db->idxlen);
    if (read(db->idxfd, db->buf->data, db->idxlen) < 0)
        db_err_sys("_db_read_idx: read error");

    if (!(datoff = strchr(db->buf->data, DB_SEP)))
        db_err_quit("_db_read_idx: missing separater [key:datoff]");
    *datoff++ = '\0';
    if (!(datlen = strchr(datoff, DB_SEP)))
        db_err_quit("_db_read_idx: missing separater [datoff:datlen]");
    *datlen++ = '\0';

    db->datoff = atol(datoff);
    db->datlen = atol(datlen);
}

static int
_db_find(DB *db, const char *key)
{
    int rc = -1;
    char nrecoff[DB_FIELD_SZ + 1];

    /* key所在的散列链头 */
    db->chainoff = db->hashoff + (_db_hash(db, key) * DB_FIELD_SZ);

    if (lseek(db->idxfd, db->chainoff, SEEK_SET) < 0)
        db_err_sys("_db_find: lseek error");
    if (read(db->idxfd, nrecoff, DB_FIELD_SZ) < 0)
        db_err_sys("_db_find: read error");

    /* 搜寻整个散列链，查找匹配的key */
    db->nrecoff = atol(nrecoff);
    while (db->nrecoff) {
        _db_read_idx(db);
        if (strcmp(db->buf->data, key) == 0) {
            rc = 0;
            break;
        }
    }

    return rc;
}

static DB_STR *
_db_read_dat(DB *db)
{
    DB_STR *dstr = db_str_init(db->datlen);

    if (lseek(db->datfd, db->datoff, SEEK_SET) < 0)
        db_err_sys("_db_read_dat: lseek error");

    if (read(db->datfd, dstr->str, dstr->len) < 0)
        db_err_sys("_db_read_dat: read error");

    return dstr;
}

DB_STR *
db_fetch(DB *db, const char *key)
{
    DB_STR *dstr;

    if (_db_find(db, key) < 0)
        return NULL;
    dstr = _db_read_dat(db);

    return dstr;
}

static void
_db_write_idx(DB *db, const char *key, off_t offset, off_t whence)
{
    char idx[DB_FIELD_SZ * 2 + 1];
    char off[DB_FIELD_SZ + 1];
    struct iovec iov[2];
    size_t len = strlen(key);

    db->chainoff = db->hashoff + (_db_hash(db, key) * DB_FIELD_SZ);

    if (lseek(db->idxfd, db->chainoff, SEEK_SET) < 0)
        db_err_sys("_db_write_idx: lseek error");

    if (read(db->idxfd, off, DB_FIELD_SZ) < 0)
        db_err_sys("_db_write_idx: read error");
    db->nrecoff = atol(off);

    if ((db->idxoff = lseek(db->idxfd, offset, whence)) < 0)
        db_err_sys("_db_write_idx: lseek error");

    db_buf_update(db->buf, len + DB_FIELD_SZ * 3 + 4);
    snprintf(db->buf->data, db->buf->max_len, "%c%s%c%lld%c%zu\n",
            DB_SEP, key, DB_SEP, db->datoff, DB_SEP, db->datlen);
    db->idxlen = strlen(db->buf->data);

    snprintf(idx, sizeof(idx), "%*lld%*zu",
            DB_FIELD_SZ, db->nrecoff, DB_FIELD_SZ, db->idxlen);
    len = strlen(idx);

    iov[0].iov_base = idx;
    iov[0].iov_len = len;
    iov[1].iov_base = db->buf->data;
    iov[1].iov_len = db->idxlen;

    if (writev(db->idxfd, iov, 2) != len + db->idxlen)
        db_err_sys("_db_write_idx: writev error");

    if (lseek(db->idxfd, db->chainoff, SEEK_SET) < 0)
        db_err_sys("_db_write_idx: lseek error");

    snprintf(off, sizeof(off), "%*lld", DB_FIELD_SZ, db->idxoff);
    if (write(db->idxfd, off, DB_FIELD_SZ) != DB_FIELD_SZ)
        db_err_sys("_db_write_idx: write error");

    if (lseek(db->idxfd, DB_FIELD_SZ * 2, SEEK_SET) < 0)
        db_err_sys("_db_write_idx: lseek error");

    snprintf(off, sizeof(off), "%*zu", DB_FIELD_SZ, db->hashnums);
    if (write(db->idxfd, off, DB_FIELD_SZ) != DB_FIELD_SZ)
        db_err_sys("_db_write_idx: write error");
}

static void
_db_write_dat(DB *db, void *data, size_t len,
        off_t offset, off_t whence)
{
    static char newline = '\n';
    struct iovec iov[2];

    db->datlen = len;
    if ((db->datoff = lseek(db->datfd, offset, whence)) < 0)
        db_err_sys("db_insert: lseek error");

    iov[0].iov_base = data;
    iov[0].iov_len = len;
    iov[1].iov_base = &newline;
    iov[1].iov_len = 1;

    if (writev(db->datfd, iov, 2) < 0)
        db_err_sys("db_insert: writev error");
    db->hashnums++;
}

void
db_insert(DB *db, const char *key, void *data, size_t len)
{
    if (_db_find(db, key) == 0)
        return;
    _db_write_dat(db, data, len, 0, SEEK_END);
    _db_write_idx(db, key, 0, SEEK_END);
}

int
main(void)
{
    char key[20];
    char data[20];

    /* DB *d = db_open("hello");
    for (int i = 0; i < 30000; i++) {
        sprintf(key, "%s%d", "jfd", i);
        sprintf(data, "%s%d", "foeito", i);
        db_insert(d, key, data, strlen(data));
    }

    for (int i = 0; i < 30000; i++) {
        sprintf(key, "%s%d", "jfd", i);
        DB_STR *s = db_fetch(d, key);

        if (s)
            printf("%s\n", s->str);
    } */

    db_close(d);
}
