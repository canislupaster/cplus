cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)
project(cplus)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(BUILD_DEBUG)
endif ()

add_executable(headergen headergen/headergen.c ./src/hashtable.c ./src/vector.c ./src/util.c ./src/siphash.c)

file(GLOB SRC headergen/test/*.[ch])
add_library(makeheaders-test ${SRC})
add_dependencies(makeheaders-test headergen)

add_custom_target(gen-headers ALL /bin/sh headergen/headergen.sh ${CMAKE_BINARY_DIR} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
add_dependencies(gen-headers headergen)

file(GLOB SRC ./src/*.h ./src/*.c)
#add_executable(cplus ${SRC})
#add_dependencies(cplus headers)

add_executable(cplus-interactive ${SRC} interactive/interactive.h interactive/interactive.c)
add_dependencies(cplus-interactive gen-headers)

enable_testing()
add_executable(hashtable_test tests/hashtable_test.c)
add_test(hashtable_test hashtable_test)