cmake_minimum_required(VERSION 3.20)

project(delta_zstd_cpp)

include(FetchContent)


set(ZSTD_BUILD_STATIC ON)
set(ZSTD_BUILD_SHARED OFF)

FetchContent_Declare(
    zstd
    URL "https://github.com/facebook/zstd/releases/download/v1.5.5/zstd-1.5.5.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR build/cmake
)

FetchContent_MakeAvailable(zstd)
set(CMAKE_CXX_STANDARD 20)

add_executable(delta_zstd_cpp src/main.cpp)


target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    libzstd_static
)

# On windows and macos this is needed
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${zstd_SOURCE_DIR}/lib
)

#------------------------------------------------------------------------------
# Enable testing with CTest
enable_testing()

add_executable(test_all test/test_all.cpp)
target_link_libraries(
    test_all
    PRIVATE
    libzstd_static
)

# On windows and macos this is needed
target_include_directories(
    test_all
    PRIVATE
    ${zstd_SOURCE_DIR}/lib
)

add_test(NAME test_all COMMAND test_all)