cmake_minimum_required(VERSION 2.6)
project(xsltproc)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories("src")
include_directories("src/zlog/src")
include_directories("test")

include_directories("/opt/local/include")

enable_testing()
add_executable(AllocatorTest test/allocator_test.c)
add_test(NAME MyTest COMMAND AllocatorTest)

add_executable(xsltproc xsltproc/turboxsltproc.c)

file(GLOB ZLOG_LIB_SOURCES src/zlog/src/*.c)
file(GLOB ZLOG_LIB_HEADERS src/zlog/src/*.h)

file(GLOB TURBOXSL_LIB_SOURCES src/*.c)
file(GLOB TURBOXSL_LIB_HEADERS src/*.h)
add_library(turboxsl ${TURBOXSL_LIB_SOURCES} ${TURBOXSL_LIB_HEADERS} ${ZLOG_LIB_SOURCES} ${ZLOG_LIB_HEADERS})
target_link_libraries(turboxsl memcached)

target_link_libraries(xsltproc turboxsl)
target_link_libraries(AllocatorTest turboxsl)
