cmake_minimum_required(VERSION 3.13)
project(ex4 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(ex4 osqueue.c osqueue.h test.c threadPool.h threadPool.c)