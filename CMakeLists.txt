cmake_minimum_required(VERSION 3.8)

#set(CMAKE_CXX_CLANG_TIDY
#    clang-tidy;
#    -checks=performance-*,bugprone-*,cppcoreguidelines-*,readability-*;)

project(main CXX)
#set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Og -ggdb")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Os")
file(GLOB SRC main.cpp)

add_executable(${PROJECT_NAME} ${SRC})

