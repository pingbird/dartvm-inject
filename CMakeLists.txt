cmake_minimum_required(VERSION 3.8)
project(dartvm_inject)

set(CMAKE_CXX_STANDARD 17)

SET (CMAKE_C_COMPILER "/usr/bin/clang-6.0")
SET (CMAKE_CXX_COMPILER "/usr/bin/clang++-6.0")

include_directories("include")
include_directories("/usr/lib/llvm-6.0/include")
include_directories("/home/ping/git/dart-sdk-stable/sdk/runtime")

add_executable(dart-inject main.cpp)
find_library(LLDB_LIBRARY NAMES lldb PATHS /usr/lib/llvm-6.0/lib)
target_link_libraries(dart-inject PUBLIC ${LLDB_LIBRARY})
add_library(antman SHARED antman.cpp)
