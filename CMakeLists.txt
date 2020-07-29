cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)
project(cplus)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(BUILD_DEBUG)
endif ()

add_subdirectory(corecommon)

add_custom_target(cplus-headers ALL ${CMAKE_BINARY_DIR}/corecommon/headergen ./src ./tests ./interactive ./playground WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_dependencies(cplus-headers headergen)

file(GLOB SRC ./src/*.h ./src/*.c)

add_executable(cplus-interactive ${SRC} interactive/interactive.c)
add_dependencies(cplus-interactive cplus-headers)