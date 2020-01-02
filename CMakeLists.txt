cmake_minimum_required(VERSION 3.14)
project(cplus)

set(CMAKE_C_STANDARD 11)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(BUILD_DEBUG)
endif ()

add_custom_target(headers ALL /bin/sh cmake-headers.sh WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

file(GLOB SRC ./src/*.h ./src/*.c)
#add_executable(cplus ${SRC})
#add_dependencies(cplus headers)

add_executable(cplus-interactive ${SRC} interactive/interactive.h interactive/interactive.c)
add_dependencies(cplus-interactive headers)

enable_testing()
add_executable(hashtable_test tests/hashtable_test.c)
add_test(hashtable_test hashtable_test)