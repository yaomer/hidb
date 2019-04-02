#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

void
db_err_sys(const char *fmt, ...)
{
    va_list ap;
    char    buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ - 1, fmt, ap);
    snprintf(buf + strlen(buf), BUFSIZ - strlen(buf) - 1,
            ": %s\n", strerror(errno));
    fputs(buf, stderr);
    va_end(ap);
    exit(1);
}

void
db_err_quit(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
