include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_executable(thread_pool_test test_thread_pool.cpp)
add_executable(socket_test test_socket.cpp)
target_link_libraries(thread_pool_test PRIVATE kvdb_lib GTest::gtest_main)
target_link_libraries(socket_test PRIVATE kvdb_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(thread_pool_test)
gtest_discover_tests(socket_test)