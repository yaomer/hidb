#ifndef _HIDB_ERROR_H
#define _HIDB_ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define db_error(...) \
    do { \
        printf("%s:%d: error: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        abort(); \
    } while (0)

#endif /* _HIDB_ERROR_H */
