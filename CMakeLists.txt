cmake_minimum_required(VERSION 3.14)
project(thingy)

set(CMAKE_CXX_STANDARD 11)

add_executable(thingy main.c)

enable_testing()
add_executable(hashtable_test hashtable_test.c)
add_test(hashtable_test hashtable_test)