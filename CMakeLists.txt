cmake_minimum_required(VERSION 3.6)
project(Found)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99 -pthread")


set(SOURCE_FILES main.c)
add_executable(Found ${SOURCE_FILES})

TARGET_LINK_LIBRARIES(Found pthread)