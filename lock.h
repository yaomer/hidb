#ifndef _CDBM_LOCK_H
#define _CDBM_LOCK_H

/* 设置读锁 */
#define db_readlock(fd, offset, whence, len) \
    db_lock((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
/* 设置写锁 */
#define db_writelock(fd, offset, whence, len) \
    db_lock((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
/* 清除锁 */
#define db_unlock(fd, offset, whence, len) \
    db_lock((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))

int db_lock(int fd, int cmd, int type, off_t offset, int whence, off_t len);

#endif /* _CDBM_LOCK_H */
