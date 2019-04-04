#ifndef _REDB_H
#define _REDB_H

#include <fcntl.h>
#include "buf.h"

/*
 * 索引文件：
 * [free list off] [hashsize] [hashnums] [hash list] [\n] 
 * 一条索引记录：
 * [nrec off] [idxlen] [key][:][datoff][:][datlen] [\n]
 */
typedef struct redb {
    char *name;         /* 建立的索引和数据文件的名字 */
    DB_BUF *buf;        /* DB内部使用的缓冲区，通常用于读取一条索引记录 */
    off_t freeoff;      /* 空闲链表的偏移 */
    size_t hashsize;    /* 当前散列表的大小 */
    size_t hashnums;    /* 当前存储的键值对数目 */
    off_t hashoff;      /* 散列表的偏移 */

    int idxfd;          /* 索引文件fd */
    size_t idxlen;      /* 索引记录的长度 */
    off_t idxoff;       /* 索引记录的偏移 */
    int datfd;          /* 数据文件fd */
    size_t datlen;      /* 数据记录的长度 */
    off_t datoff;       /* 数据记录的偏移 */

    int lock;           /* 是否对数据库加锁 */
    off_t nrecoff;      /* 指向下一个索引记录的指针(偏移) */
    off_t chainoff;     /* 某个散列链的偏移 */
} DB;

#define DB_HASH_INIT_SIZE 256

#define DB_LOCK 1
#define DB_UNLOCK 0

/* 某些字段的固定宽度 */
#define DB_FIELD_SZ 8

#define DB_HASH_OFF (DB_FIELD_SZ * 3) 

#define DB_FREE_OFF 0

/* 打开数据库文件使用的默认方式 */
#define DB_OPEN_MODE (O_RDWR | O_CREAT)
/* 打开数据库文件使用的默认权限 */
#define DB_FILE_MODE 0660

/* 索引记录中使用的分隔符，这里使用':' */
#define DB_SEP ':'

#endif /* _REDB_H */
