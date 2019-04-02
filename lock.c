#include <sys/fcntl.h>
#include "lock.h"

int
db_lock(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock f;

    f.l_type = type;
    f.l_start = offset;
    f.l_whence = whence;
    f.l_len = len;

    return fcntl(fd, cmd, &f);
}
