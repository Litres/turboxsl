cmake_minimum_required(VERSION 2.6)
project(xsltproc)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories("src")

add_executable(xsltproc xsltproc/turboxsltproc.c)

file(GLOB TURBOXSL_LIB_SOURCES src/*.c)
file(GLOB TURBOXSL_LIB_HEADERS src/*.h)
add_library(turboxsl ${TURBOXSL_LIB_SOURCES} ${TURBOXSL_LIB_HEADERS})

target_link_libraries(xsltproc turboxsl)
