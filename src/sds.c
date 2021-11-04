#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sds.h"

sds_t *sds_new()
{
    sds_t *s = db_malloc(sizeof(sds_t));
    s->buf = NULL;
    s->len = 0;
    return s;
}

void sds_free(sds_t *sds)
{
    db_free(sds->buf);
    db_free(sds);
}

void sds_init(sds_t *s)
{
    s->buf = NULL;
    s->len = 0;
}

void sds_clear(sds_t *s)
{
    db_free(s->buf);
}

void sds_resize(sds_t *s, size_t size)
{
    s->buf = db_realloc(s->buf, size);
    s->len = size;
}

sds_t *sds_dup(const char *s)
{
    return sds_dup2(s, strlen(s));
}

sds_t *sds_dup2(const char *s, size_t len)
{
    sds_t *sds = sds_new();
    sds_append2(sds, s, len);
    return sds;
}

sds_t *sds_dup3(sds_t *sds)
{
    return sds_dup2(sds->buf, sds->len);
}

void sds_append(sds_t *sds, const char *s)
{
    sds_append2(sds, s, strlen(s));
}

void sds_append2(sds_t *sds, const char *s, size_t len)
{
    sds->buf = db_realloc(sds->buf, sds->len + len);
    memcpy(sds->buf + sds->len, s, len);
    sds->len += len;
}

int sds_cmp(sds_t *s1, sds_t *s2)
{
    if (s1->len < s2->len) return -1;
    if (s1->len > s2->len) return 1;
    return memcmp(s1->buf, s2->buf, s1->len);
}

const char *sds2str(sds_t *s)
{
    sds_append2(s, "\0", 1);
    s->len -= 1;
    return s->buf;
}

size_t sds_hash(sds_t *s)
{
    size_t hv = 0;
    for (int i = 0; i < s->len; i++) {
        hv = hv * 31 + s->buf[i];
    }
    return hv;
}

char *concat(const char *s1, const char *s2)
{
    size_t len1 = strlen(s1), len2 = strlen(s2);
    char *s = db_malloc(len1 + len2 + 1);
    strcpy(s, s1);
    strcpy(s + len1, s2);
    return s;
}
