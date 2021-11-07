#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "../include/sds.h"

sds_t *sds_new()
{
    sds_t *s = db_malloc(sizeof(sds_t));
    s->buf = NULL;
    s->len = 0;
    s->avail = 0;
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
    s->avail = 0;
}

void sds_clear(sds_t *s)
{
    db_free(s->buf);
}

static void sds_growth(sds_t *s, size_t fit_size)
{
    static const int incr_factor = 2;
    size_t size = s->len + s->avail;
    if (size == 0) size = 2;
    size_t n = size * incr_factor;
    while (n < fit_size) n *= incr_factor;
    s->buf = db_realloc(s->buf, n);
    s->avail = n - s->len;
}

void sds_resize(sds_t *s, size_t size)
{
    if (size > s->len + s->avail) {
        sds_growth(s, size);
    }
    s->avail = (s->len + s->avail) - size;
    s->len = size;
}

void sds_reserve(sds_t *s, size_t size)
{
    if (size <= s->avail) return;
    sds_growth(s, s->len + size);
}

void sds_append(sds_t *sds, const char *s, size_t len)
{
    if (len > sds->avail) {
        sds_growth(sds, sds->len + len);
    }
    memcpy(sds->buf + sds->len, s, len);
    sds->len += len;
    sds->avail -= len;
}

int sds_cmp(sds_t *s1, sds_t *s2)
{
    if (s1->len < s2->len) return -1;
    if (s1->len > s2->len) return 1;
    return memcmp(s1->buf, s2->buf, s1->len);
}

const char *sds2str(sds_t *s)
{
    sds_append(s, "\0", 1);
    s->len -= 1;
    return s->buf;
}

size_t strhash(const char *s, size_t len)
{
    size_t hv = 0;
    for (size_t i = 0; i < len; i++) {
        hv = hv * 31 + s[i];
    }
    return hv;
}
