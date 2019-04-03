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
#include "hash.h"
#include "buf.h"
#include "error.h"

static DB *
_db_alloc_db(size_t len)
{
    DB *db = db_calloc(1, sizeof(DB));

    db->idx_fd = db->dat_fd = -1;
    db->hash = db_hash_init();
    /* for [.idx/dat] and ['\0'] */
    db->name = db_malloc(len + 4 + 1);
    db->buf = db_buf_init();

    return db;
}

DB *
db_open(const char *name)
{
    size_t len = strlen(name);
    DB *db = _db_alloc_db(len);
    struct stat statbuf;
    /* [freeptr] [hashsize] [hashnums] [\n\0] */
    char idx[DB_FIELD_SZ * 3 + 2];

    strcpy(db->name, name);
    strcat(db->name, ".idx");

    db->hash_off = DB_FIELD_SZ * 3;

    if ( (db->idx_fd = open(db->name, DB_OPEN_MODE, DB_FILE_MODE)) < 0)
        db_err_sys("db_open: open error");
    strcpy(db->name + len, ".dat");
    if ( (db->dat_fd = open(db->name, DB_OPEN_MODE, DB_FILE_MODE)) < 0)
        db_err_sys("db_open: open error");

    if (db_writelock(db->idx_fd, 0, SEEK_SET, 0) < 0)
        db_err_quit("db_open: db_writelock error");

    if (fstat(db->idx_fd, &statbuf) < 0)
        db_err_sys("db_open: fstat error");

    if (statbuf.st_size == 0) {
        snprintf(idx, sizeof(idx), "%*d%*zu%*zu\n",
                DB_FIELD_SZ, 0, DB_FIELD_SZ, db->hash->hsize, DB_FIELD_SZ, db->hash->hnums);
        ssize_t n = strlen(idx);
        if (write(db->idx_fd, idx, n) != n)
            db_err_sys("db_open: write error");
    }

    if (db_unlock(db->idx_fd, 0, SEEK_SET, 0) < 0)
        db_err_quit("db_open: db_unlock error");
    return db;
}

static unsigned
_db_hash(DB *db, const char *key)
{
    unsigned hval;

    for (hval = 0; *key != '\0'; key++)
        hval = *key + 31 * hval;
    return hval % db->hash->hsize;
}

static int
_db_find_lock(DB *db, const char *key, int writelock)
{
    off_t offset, nextoff;

    db->chain_off = (_db_hash(db, key) * DB_FIELD_SZ) + db->hash_off;
    db->nrec_ptr = db->chain_off;

    if (writelock) {
        if (db_writelock(db->idx_fd, db->chain_off, SEEK_SET, 1) < 0)
            db_err_quit("_db_find_lock: db_writelock error");
    } else {
        if (db_readlock(db->idx_fd, db->chain_off, SEEK_SET, 1) < 0)
            db_err_quit("_db_find_lock: db_readlock error");
    }

    offset = _db_read_ptr(db, db->nrec_ptr);
    while (offset) {
        nextoff = _db_read_idx(db, offset);
        if (strcmp(db->buf, key) == 0)
            break;
        db->nrec_ptr = offset;
        offset = nextoff;
    }

    return offset ? 1 : -1;
}

/*
 * read [freeptr] [hashsize] [hashnums] [hash list head]
 */
static off_t
_db_read_ptr(DB *db, off_t offset)
{
    char off[DB_FIELD_SZ + 1];

    if (lseek(db->idx_fd, offset, SEEK_SET) < 0)
        db_err_sys("_db_read_ptr: lseek error");
    if (read(db->idx_fd, off, DB_FIELD_SZ) != DB_FIELD_SZ)
        db_err_sys("_db_read_ptr: read error");

    return atol(off);
}

/*
 * 读取一条索引记录，并返回指向下一条索引记录的指针
 */
static off_t
_db_read_idx(DB *db, off_t offset)
{
    char nrec_ptr[DB_FIELD_SZ + 1];
    char idx_len[DB_FIELD_SZ + 1];
    struct iovec iov[2];
    char *ptr1, *ptr2;

    if (lseek(db->idx_fd, offset, SEEK_SET) < 0)
        db_err_sys("_db_read_idx: lseek error");

    iov[0].iov_base = nrec_ptr;
    iov[0].iov_len = DB_FIELD_SZ;
    iov[1].iov_base = idx_len;
    iov[1].iov_len = DB_FIELD_SZ;

    /* read [nrec_ptr] and [idxlen] */
    if (readv(db->idx_fd, iov, 2) != DB_FIELD_SZ * 2)
        db_err_sys("_db_read_idx: readv error");

    db->nrec_ptr = atol(nrec_ptr);
    db->idx_len = atol(idx_len);

    db_buf_update(db->buf, db->idx_len);
    /* read idx record */
    if (read(db->idx_fd, db->buf->data, db->idx_len) != db->idx_len)
        db_err_sys("_db_read_idx: read error");
    db->buf[db->idx_len] = '\0';

    /* db->buf->data: [key\0datoff\0datlen\0] */
    if ((ptr1 = strchr(db->buf->data, SEP)) == NULL)
        db_err_quit("_db_read_idx: missing sep [key:datoff]");
    *ptr1++ = '\0';   /* point to datoff */
    if ((ptr2 = strchr(db->buf->data, SEP)) == NULL)
        db_err_quit("_db_read_idx: missing sep [datoff:datlen]");
    *ptr2++ = '\0';  /* point to datlen */

    db->dat_off = atol(ptr1);
    db->dat_len = atol(ptr2);

    return nrec_ptr;
}

static DB_STR *
_db_read_dat(DB *db)
{
    DB_STR *dstr = db_str_init(db->dat_len);

    if (lseek(db->dat_fd, db->dat_off, SEEK_SET) < 0)
        db_err_sys("_db_read_dat: lseek error");
    if (read(db->dat_fd, dstr->str, dstr->len) != dstr->len)
        db_err_sys("_db_read_dat: read error");

    return dstr;
}

DB_STR *
db_fetch(DB *db, const char *key)
{
    DB_STR *dstr;

    if (_db_find_lock(db, key, 0) > 0)
        dstr = _db_read_dat(db);
    else
        dstr = NULL;

    if (db_unlock(db->idx_fd, db->chain_off, SEEK_SET, 1) < 0)
        db_err_quit("db_fetch: db_unlock error");
    return dstr;
}

static void
_db_do_delete(DB *db)
{

}

int
db_delete(DB *db, const char *key)
{
    int rc;

    if (_db_find_lock(db, key, 1) > 0) {
        _db_do_delete(db);
        rc = 0;
    } else
        rc = -1;

    if (db_unlock(db->idx_fd, db->chain_off, SEEK_SET, 1) < 0)
        db_err_quit("db_delete: db_unlock error");
    return rc;
}

static void
_db_write_ptr(DB *db, off_t offset, off_t nrec_ptr)
{
    char ptr[DB_FIELD_SZ + 1];

    snprintf(ptr, sizeof(ptr), "%*zu", DB_FIELD_SZ, nrec_ptr);
    if (lseek(db->idx_fd, offset, SEEK_SET) < 0)
        db_err_sys("_db_write_ptr: lseek error");
    if (write(db->idx_fd, ptr, DB_FIELD_SZ) != DB_FIELD_SZ)
        db_err_sys("_db_write_ptr: write error");
}

static void
_db_write_idx(DB *db, const char *key,
        off_t offset, off_t whence, off_t nrec_ptr)
{
    size_t len = strlen(key);
    size_t idxlen;
    char ptr[DB_FIELD_SZ * 2 + 1];
    struct iovec iov[2];

    if (whence == SEEK_END) {
        if (db_writelock(db->idx_fd,
                        ((db->hash->hsize + 3) * DB_FIELD_SZ) + 1,
                        SEEK_SET, 0) < 0)
            db_err_quit("_db_write_idx: db_writelock error");
    }

    db_buf_update(db->buf, DB_FIELD_SZ * 2 + len + 2 + 2);
    snprintf(db->buf->data, sizeof(db->buf->data),
            "%s%c%*zu%c%*zu", key, SEP,
            DB_FIELD_SZ, db->dat_off, SEP, DB_FIELD_SZ, db->dat_len);
    idxlen = strlen(db->buf->data);
    snprintf(ptr, sizeof(ptr), "%*zu%*zu",
            DB_FIELD_SZ, nrec_ptr, DB_FIELD_SZ, idxlen);
    len = strlen(ptr);

    iov[0].iov_base = ptr;
    iov[0].iov_len = len;
    iov[1].iov_base = db->buf->data;
    iov[1].iov_len = idxlen;

    if (writev(db->idx_fd, iov, 2) != len + idxlen)
        db_err_sys("_db_write_idx: writev error");

    if (whence == SEEK_END) {
        if (db_unlock(db->idx_fd,
                     ((db->hash->hsize + 3) * DB_FIELD_SZ) + 1,
                     SEEK_SET, 0) < 0)
            db_err_quit("_db_write_idx: db_unlock error");
    }
}

static void
_db_write_dat(DB *db, void *data, size_t len,
        off_t offset, off_t whence)
{
    struct iovec iov[2];
    static char newline = '\n';

    if (whence == SEEK_END) {
        if (db_writelock(db->dat_fd, 0, SEEK_SET, 0) < 0)
            db_err_quit("_db_write_dat: db_writelock error");
    }

    if ((db->dat_off = lseek(db->dat_fd, offset, whence)) < 0)
        db_err_sys("_db_write_dat: lseek error");
    db->dat_len = len;

    iov[0].iov_base = data;
    iov[0].iov_len = len;
    iov[1].iov_base = &newline;
    iov[1].iov_len = 1;

    if (writev(db->dat_fd, iov, 2) != db->dat_len + 1)
        db_err_sys("_db_write_dat: writev error");

    if (whence == SEEK_END) {
        if (db_unlock(db->dat_fd, 0, SEEK_SET, 0) < 0)
            db_err_quit("_db_write_dat: db_unlock error");
    }
}

static int
_db_find_free_lock(DB *db, size_t keylen, size_t datlen)
{
    off_t offset, nextoff, saveoff;
    int rc = 0;

    if (db_writelock(db->idx_fd, DB_FREE_OFF, SEEK_SET, 1) < 0)
        db_err_quit("_db_find_free_lock: db_writelock error");

    saveoff = DB_FREE_OFF;
    offset = _db_read_ptr(db, saveoff);
    while (offset) {
        nextoff = _db_read_idx(db, offset);
        if (strlen(db->buf->data) == keylen && db->dat_len == datlen)
            break;
        saveoff = offset;
        offset = nextoff;
    }

    if (offset == 0)
        rc = -1;
    else {
        _db_write_ptr(db, saveoff, db->nrec_ptr);
        rc = 0;
    }

    if (db_unlock(db->idx_fd, DB_FREE_OFF, SEEK_SET, 1) < 0)
        db_err_quit("_db_find_free_lock: db_unlock error");
    return rc;
}

int
db_store(DB *db, const char *key, void *data,
        size_t datlen, int flag)
{
    int rc;
    size_t keylen = strlen(key);
    off_t nrec_ptr;

    switch (flag) {
    case DB_INSERT:
        if (_db_find_lock(db, key, 1) > 0) {
            rc = -1;
            goto ret;
        }

        nrec_ptr = _db_read_ptr(db, db->chain_off);

        if (_db_find_free_lock(db, keylen, datlen) < 0) {
            _db_write_dat(db, data, datlen, 0, SEEK_END);
            _db_write_idx(db, key, 0, SEEK_END, nrec_ptr);
        } else {
            _db_write_dat(db, data, datlen, db->dat_off, SEEK_SET);
            _db_write_idx(db, key, db->idx_off, SEEK_SET, nrec_ptr);
            _db_write_ptr(db, db->chain_off, db->idx_off);
        }
        break;
    case DB_REPLACE:
        if (_db_find_lock(db, key, 1) < 0) {
            rc = -1;
            goto ret;
        }
        break;
    case DB_STORE:
        break;
    default:
        break;
    }

ret:
    if (db_unlock(db->idx_fd, db->chain_off, SEEK_SET, 1) < 0)
        db_err_quit("db_store: db_unlock error");
    return rc;
}

int
main(void)
{
    DB *db = db_open("hello");

}
