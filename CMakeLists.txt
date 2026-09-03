cmake_minimum_required(VERSION 3.20)
project(kvdb CXX)

option(BUILD_BENCHMARK "Build the load-testing benchmark tool (fetches HdrHistogram_c)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_FLAGS_TSAN "-g -O1 -fsanitize=thread"
    CACHE STRING "Flags for ThreadSanitizer build" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_TSAN "-fsanitize=thread"
    CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_ASAN "-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer"
    CACHE STRING "Flags for Address/UB Sanitizer build" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined"
    CACHE STRING "" FORCE)

mark_as_advanced(CMAKE_CXX_FLAGS_TSAN CMAKE_EXE_LINKER_FLAGS_TSAN
                 CMAKE_CXX_FLAGS_ASAN CMAKE_EXE_LINKER_FLAGS_ASAN)

add_library(kvdb_lib STATIC
    src/thread_pool.cpp
    src/socket.cpp
    src/tcp_server.cpp
    src/store.cpp
    src/protocol.cpp
    src/handler.cpp
    src/logger.cpp
)
target_include_directories(kvdb_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(kvdb_lib PRIVATE -Wall -Wextra -Wpedantic)
find_package(Threads REQUIRED)
target_link_libraries(kvdb_lib PUBLIC Threads::Threads)

add_executable(kvdb src/main.cpp)
target_link_libraries(kvdb PRIVATE kvdb_lib)

enable_testing()
add_subdirectory(tests)

if(BUILD_BENCHMARK)
    add_subdirectory(tools)
endif()