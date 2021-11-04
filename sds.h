#ifndef _REDB_SDS_H
#define _REDB_SDS_H

#include <sys/types.h>

struct sds {
    char *buf;
    size_t len;
};

typedef struct sds sds_t;

sds_t *sds_new();
void   sds_free(sds_t *s);
/* Init or free a sds on the stack */
void   sds_init(sds_t *s);
void   sds_clear(sds_t *s);

void   sds_resize(sds_t *s, size_t size);
sds_t *sds_dup(const char *s);
sds_t *sds_dup2(const char *s, size_t len);
sds_t *sds_dup3(sds_t *s);
void   sds_append(sds_t *sds, const char *s);
void   sds_append2(sds_t *sds, const char *s, size_t len);
int    sds_cmp(sds_t *s1, sds_t *s2);

const char *sds2str(sds_t *s);
size_t      sds_hash(sds_t *s);

char *concat(const char *s1, const char *s2);

#endif /* _REDB_SDS_H */
