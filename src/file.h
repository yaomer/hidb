#ifndef _HIDB_FILE_H
#define _HIDB_FILE_H

#include "../include/sds.h"

struct writable_file {
    sds_t  *buf; /* write cache */
    char   *name;
    int     fd;
};

struct writable_file *new_writable_file(const char *filename);
/* Return 0 if ok */
int writable_file_append(struct writable_file *f, const char *buf, size_t len);
/* Return 0 if ok */
int writable_file_sync(struct writable_file *f);
/* Return 0 if ok */
int writable_file_close(struct writable_file *f);

#endif /* _HIDB_FILE_H */
