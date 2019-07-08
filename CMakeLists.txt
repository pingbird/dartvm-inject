cmake_minimum_required(VERSION 3.7)
project(dartvm_inject)

set(CMAKE_CXX_STANDARD 14)

include_directories("include")
include_directories("/usr/lib/llvm-6.0/include")

add_executable(dart-inject main.cpp)
find_library(LLDB_LIBRARY NAMES lldb PATHS /usr/lib/llvm-6.0/lib)
target_link_libraries(dart-inject PUBLIC ${LLDB_LIBRARY})
add_library(thanos SHARED thanos.cpp)
