cmake_minimum_required(VERSION 3.8)
project(main CXX)
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")
#set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=readability-*,performance-*")
file(GLOB SRC main.cpp)

add_executable(${PROJECT_NAME} ${SRC})

