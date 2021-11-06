cmake_minimum_required (VERSION 2.8)

project (hidb)

set (CMAKE_C_FLAGS "-std=c11 -Wall -O2")

include (CheckSymbolExists)

CHECK_SYMBOL_EXISTS(fdatasync "unistd.h" HAVE_FDATASYNC)
CHECK_SYMBOL_EXISTS(F_FULLFSYNC "fcntl.h" HAVE_FULLFSYNC)

set (SRC "${PROJECT_SOURCE_DIR}/src")
set (PUBLIC_HEADER_DIR "${PROJECT_SOURCE_DIR}/include")

configure_file (
    "${SRC}/config.h.in"
    "${SRC}/config.h"
)

set (DB
    ${SRC}/alloc.c
    ${SRC}/sds.c
    ${SRC}/hash.c
    ${SRC}/concurrency_hash.c
    ${SRC}/db_impl.c
    ${SRC}/log.c
    ${SRC}/file.c
)

set (HEADERS
    ${PUBLIC_HEADER_DIR}/db.h
    ${PUBLIC_HEADER_DIR}/sds.h
)

add_library (hidb STATIC ${DB})

install(TARGETS hidb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(FILES ${HEADERS} DESTINATION include/hidb)

