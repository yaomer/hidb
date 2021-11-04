cmake_minimum_required (VERSION 2.8)

project (hidb)

set (CMAKE_C_FLAGS "-std=c11 -Wall -O2")

set (SRC "${PROJECT_SOURCE_DIR}/src")

set (DB
    ${SRC}/alloc.c
    ${SRC}/sds.c
    ${SRC}/hash.c
    ${SRC}/concurrency_hash.c
    ${SRC}/db.c
    ${SRC}/log.c
    ${SRC}/error.c
)

set (HEADERS
    ${SRC}/db.h
    ${SRC}/sds.h
)

add_library (hidb STATIC ${DB})

install(TARGETS hidb
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(FILES ${HEADERS} DESTINATION include/hidb)

