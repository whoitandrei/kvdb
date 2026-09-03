include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
function(add_kvdb_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE kvdb_lib GTest::gtest_main)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)

    gtest_discover_tests(${target}
        PROPERTIES
            TIMEOUT 60
            LABELS "unit"
        DISCOVERY_TIMEOUT 30
    )
endfunction()

add_kvdb_test(thread_pool_test test_thread_pool.cpp)
add_kvdb_test(socket_test      test_socket.cpp)
add_kvdb_test(tcp_server_test  test_tcp_server.cpp)
add_kvdb_test(task_test        test_task.cpp)
add_kvdb_test(store_test       test_store.cpp)
add_kvdb_test(protocol_test    test_protocol.cpp)
