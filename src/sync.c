#include <unistd.h>
#include <fcntl.h>

#include "config.h"

int sync_fd(int fd)
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
