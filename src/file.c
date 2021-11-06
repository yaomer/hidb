#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "alloc.h"
#include "file.h"

static int sync_fd(int fd)
{
#if defined (HAVE_FULLFSYNC)
    /* Just using fsync() is not safe on Mac OS(See man 2 fsync) */
    return fcntl(fd, F_FULLFSYNC);
#endif
#if defined (HAVE_FDATASYNC)
    /* Without changing the file size,
     * fdatasync() costs less to update metadata once than fsync()
     */
    return fdatasync(fd);
#else
    /* We fallback to fsync() */
    return fsync(fd);
#endif
}

static int write_fd(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return n;
        }
        buf += n;
        len -= n;
    }
    return len;
}

static const size_t write_buffer_size = 1024 * 16;

static int flush_buf(struct writable_file *f)
{
    int ret = write_fd(f->fd, f->buf->buf, f->buf->len);
    if (ret) return ret;
    sds_resize(f->buf, 0);
    return ret;
}

struct writable_file *new_writable_file(const char *filename)
{
    struct writable_file *f = db_malloc(sizeof(struct writable_file));
    f->name = db_strdup(filename);
    f->fd = open(f->name, O_RDWR | O_APPEND | O_CREAT, 0644);
    f->buf = sds_new();
    sds_reserve(f->buf, write_buffer_size);
    return f;
}

int writable_file_append(struct writable_file *f, const char *buf, size_t len)
{
    if (len >= write_buffer_size) {
        return write_fd(f->fd, buf, len);
    }
    sds_append(f->buf, buf, len);
    if (f->buf->len >= write_buffer_size) {
        return flush_buf(f);
    }
    return 0;
}

int writable_file_sync(struct writable_file *f)
{
    int ret = 0;
    if (f->buf->len > 0) ret = flush_buf(f);
    return ret || sync_fd(f->fd);
}

int writable_file_close(struct writable_file *f)
{
    int ret = writable_file_sync(f);
    if (ret) return ret;
    sds_free(f->buf);
    db_free(f->name);
    return close(f->fd);
}
