cmake_minimum_required(VERSION 3.10)

# Benchmark executable
add_executable(string_benchmark EXCLUDE_FROM_ALL benchmark_main.cpp)

# Ensure benchmark library is built first
add_dependencies(string_benchmark benchmark)

# Compiler flags for benchmarks - disable warnings for optimization
list(APPEND BENCHMARK_FLAGS
    -O3
    -DNDEBUG
    -march=native
    -Wno-error
    -Wno-uninitialized
    -Wno-deprecated-declarations
)

target_compile_options(string_benchmark PRIVATE ${BENCHMARK_FLAGS})

# Link libraries - use the benchmark library
target_link_libraries(string_benchmark PRIVATE 
    pthread
    benchmark
)

# Force linking with libc++ when using Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(string_benchmark PRIVATE -stdlib=libc++)
    target_link_options(string_benchmark PRIVATE -stdlib=libc++)
endif()

# Include directories
target_include_directories(string_benchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/benchmark/include
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