cmake_minimum_required(VERSION 3.14)
project(cplus)

set(CMAKE_C_STANDARD 11)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(BUILD_DEBUG)
endif ()

add_executable(makeheaders makeheaders/makeheaders.c)

file(GLOB SRC ./makeheaders/test/*.[ch])
add_library(makeheaders-test ${SRC})
add_dependencies(makeheaders-test makeheaders)

add_custom_target(gen-headers ALL /bin/sh makeheaders/cmake-headers.sh ${CMAKE_BINARY_DIR} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_dependencies(gen-headers makeheaders)

file(GLOB SRC ./src/*.h ./src/*.c)
#add_executable(cplus ${SRC})
#add_dependencies(cplus headers)

add_executable(cplus-interactive ${SRC} interactive/interactive.h interactive/interactive.c)
add_dependencies(cplus-interactive gen-headers)

enable_testing()
add_executable(hashtable_test tests/hashtable_test.c)
add_test(hashtable_test hashtable_test)