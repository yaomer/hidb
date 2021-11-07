#ifndef _HIDB_SDS_H
#define _HIDB_SDS_H

#include <sys/types.h>

/*
 * +-----------+
 * | hello | 3 |
 * +-----------+
 * (len = 5, avail = 3)
 */
struct sds {
    char *buf;
    size_t len; /* the length of the buf */
    size_t avail; /* the amount of space left available */
};

typedef struct sds sds_t;

sds_t *sds_new();
void   sds_free(sds_t *s);
/* Init or free a sds on the stack */
void   sds_init(sds_t *s);
void   sds_clear(sds_t *s);

/* set sds.len = size, don't clear mem */
void   sds_resize(sds_t *s, size_t size);
/* set sds.avail >= size */
void   sds_reserve(sds_t *s, size_t size);
void   sds_append(sds_t *sds, const char *s, size_t len);
/* Binary byte by byte comparison */
int    sds_cmp(sds_t *s1, sds_t *s2);

/* Return a C-style str.
 * Notes that sds.len does not contain trailing '\0'.
 */
const char *sds2str(sds_t *s);

size_t  strhash(const char *s, size_t len);

/* Convenient to pass */
struct slice {
    char   *buf;
    size_t  len;
};

#define slice_init(__s, __p, __l) \
    do { \
        (__s).buf = (char*)(__p); \
        (__s).len = (__l); \
    } while (0)

#endif /* _HIDB_SDS_H */
