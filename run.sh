#/bin/bash

cc redb.c buf.c lock.c alloc.c error.c -o db
./db
