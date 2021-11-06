#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

static atomic_size_t memory_used = 0;

static const size_t header_size = sizeof(size_t);

size_t db_memory_used(void)
{
    return atomic_load_explicit(&memory_used, memory_order_relaxed);
}

static void *memory_used_add(void *ptr, size_t size)
{
    atomic_fetch_add_explicit(&memory_used, size + header_size, memory_order_relaxed);
    memcpy(ptr, &size, sizeof(size));
    return (char*)ptr + header_size;
}

static void memory_used_sub(void *ptr)
{
    size_t alloced_size = *(size_t*)ptr + header_size;
    atomic_fetch_sub_explicit(&memory_used, alloced_size, memory_order_relaxed);
}

static void oom_handler(size_t size)
{
    fprintf(stderr, "out of memory when malloc(%zu)", size);
    abort();
}

void *db_malloc(size_t size)
{
    size_t alloced_size = size + header_size;
    void *ptr = malloc(alloced_size);
    if (!ptr) oom_handler(alloced_size);
    return memory_used_add(ptr, size);
}

void *db_calloc(size_t size)
{
    size_t alloced_size = size + header_size;
    void *ptr = calloc(1, alloced_size);
    if (!ptr) oom_handler(alloced_size);
    return memory_used_add(ptr, size);
}

void *db_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) return db_malloc(size);
    ptr = (char*)ptr - header_size;
    memory_used_sub(ptr);
    size_t alloced_size = size + header_size;
    void *new_ptr = realloc(ptr, alloced_size);
    if (!new_ptr) oom_handler(alloced_size);
    return memory_used_add(new_ptr, size);
}

char *db_strdup(const char *s)
{
    size_t len = strlen(s);
    char *buf = db_malloc(len + 1);
    strcpy(buf, s);
    return buf;
}

void db_free(void *ptr)
{
    if (ptr == NULL) return;
    ptr = (char*)ptr - header_size;
    memory_used_sub(ptr);
    free(ptr);
}
