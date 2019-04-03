#/bin/bash

cc redb.c hash.c buf.c lock.c alloc.c error.c -o db
./db
