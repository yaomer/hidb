#include <stdlib.h>
#include "alloc.h"
#include "error.h"

void *
db_malloc(size_t len)
{
    void *ptr;

    if ((ptr = malloc(len)) == NULL)
        db_err_sys("db_malloc: malloc error");
    return ptr;
}

void *
db_calloc(size_t n, size_t len)
{
    void *ptr;

    if ((ptr = calloc(n, len)) == NULL)
        db_err_sys("db_calloc: calloc error");
    return ptr;
}

void
db_free(void *ptr)
{
    free(ptr);
}
