cmake_minimum_required(VERSION 3.25)
project(Vordr VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure GCC reflection flags are added
add_compile_options(-freflection -Wall -Wextra -pthread)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Vordr library
add_library(vordr STATIC
    src/logger.cpp
    src/config.cpp
    src/xml_parser.cpp
    src/mmap_sink.cpp
)

target_include_directories(vordr PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(vordr PUBLIC pthread)

# Test target
enable_testing()

add_executable(vordr_tests
    tests/test_main.cpp
    tests/test_reflection.cpp
    tests/test_stream_buffer.cpp
    tests/test_config.cpp
    tests/test_mmap_sink.cpp
    tests/test_lazy_eval.cpp
    tests/test_integration.cpp
)

target_link_libraries(vordr_tests PRIVATE vordr pthread)

add_test(NAME vordr_tests COMMAND vordr_tests)

# Example target
add_executable(vordr_example examples/basic_usage.cpp)
target_link_libraries(vordr_example PRIVATE vordr pthread)

# Benchmark target
add_executable(vordr_benchmarks tests/test_benchmark.cpp)
target_link_libraries(vordr_benchmarks PRIVATE vordr pthread)
