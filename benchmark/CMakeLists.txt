cmake_minimum_required(VERSION 3.10)

# Find Google Benchmark
find_package(PkgConfig REQUIRED)
pkg_check_modules(BENCHMARK REQUIRED benchmark)

# Benchmark executable
add_executable(string_benchmark EXCLUDE_FROM_ALL benchmark_main.cpp)

# Compiler flags for benchmarks - disable warnings for optimization
list(APPEND BENCHMARK_FLAGS
    -O3
    -DNDEBUG
    -march=native
    -Wno-error
    -Wno-maybe-uninitialized
    -Wno-uninitialized
)

target_compile_options(string_benchmark PRIVATE ${BENCHMARK_FLAGS})

# Link libraries
target_link_libraries(string_benchmark PRIVATE 
    pthread
    ${BENCHMARK_LIBRARIES}
)

# Include directories
target_include_directories(string_benchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${BENCHMARK_INCLUDE_DIRS}
)

# Add benchmark target to build all benchmarks
add_custom_target(benchmark_target 
    DEPENDS string_benchmark
    COMMENT "Building all benchmark executables"
)

# Add run target for convenience
add_custom_target(run_benchmark
    COMMAND string_benchmark
    DEPENDS string_benchmark
    COMMENT "Running string benchmarks"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)