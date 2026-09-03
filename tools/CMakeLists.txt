include(FetchContent)

FetchContent_Declare(
    hdr_histogram
    GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
    GIT_TAG 0.11.8
)
FetchContent_MakeAvailable(hdr_histogram)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE hdr_histogram_static)
target_include_directories(benchmark PRIVATE ${hdr_histogram_SOURCE_DIR}/src)
target_include_directories(benchmark PRIVATE ${hdr_histogram_SOURCE_DIR}/include)