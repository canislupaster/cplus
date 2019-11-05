cmake_minimum_required(VERSION 3.14)
project(cplus)

set(CMAKE_CXX_STANDARD 11)

add_executable(cplus src/main.c)

enable_testing()
add_executable(hashtable_test tests/hashtable_test.c)
add_test(hashtable_test hashtable_test)