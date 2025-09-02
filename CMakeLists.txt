#[[
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
]]

cmake_minimum_required(VERSION 3.00)

project(small)

set(CMAKE_CXX_STANDARD 20)
set(COMMON_WARNING_FLAGS "${COMMON_WARNING_FLAGS} -Wall -Werror -Wextra -Wendif-labels -Wshadow -Wunused-parameter -Wunused-variable  -Wparentheses -Wuninitialized -Wcomment -Wformat -Wimplicit-fallthrough -Wsign-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} -frtti -fexceptions -fsized-deallocation")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# sanitizer
set(SANITIZE_PARAM "-fsanitize=address -fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer")  # param from seastar.pc
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${SANITIZE_PARAM}")
set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${SANITIZE_PARAM}")

OPTION(CODE_COVERAGE
       "build with code coverage, only work in DEBUG"
        OFF
        )

set(SMALL_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
include_directories(SYSTEM ${SMALL_SOURCE_DIR}/doctest/)
include_directories(${SMALL_SOURCE_DIR}/)

## code coverage

# code coverage section **must** place before all subdirectories to test coverage.
if((${CMAKE_BUILD_TYPE} MATCHES Debug) AND CODE_COVERAGE)
  # Add coverage compiler flags
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

  # Find required tools
  find_program(GCOV_PATH gcov)
  find_program(GCOVR_PATH gcovr)
  
  if(NOT GCOV_PATH)
    message(FATAL_ERROR "gcov not found! Aborting...")
  endif()
  
  if(NOT GCOVR_PATH)
    message(FATAL_ERROR "gcovr not found! Aborting...")
  endif()

  # Coverage target
  add_custom_target(coverage
    # Clean previous coverage data
    COMMAND find . -name "*.gcda" -exec rm {} +
    
    # Run tests
    COMMAND ${CMAKE_COMMAND} --build . --target unit_tests
    COMMAND ./unit/unit_tests
    
    # Generate HTML coverage report
    COMMAND ${GCOVR_PATH} --root ${CMAKE_SOURCE_DIR} 
            --exclude ".*doctest.*"
            --exclude ".*unit/.*"
            --exclude ".*regression/.*"
            --exclude ".*build.*"
            --exclude ".*/usr/.*"
            --html --html-details
            --output coverage.html
            --print-summary
            --exclude-unreachable-branches
            --exclude-throw-branches
    
    # Generate XML coverage report for CI
    COMMAND ${GCOVR_PATH} --root ${CMAKE_SOURCE_DIR}
            --exclude ".*doctest.*" 
            --exclude ".*unit/.*"
            --exclude ".*regression/.*"
            --exclude ".*build.*"
            --exclude ".*/usr/.*"
            --xml --output coverage.xml
            --exclude-unreachable-branches
            --exclude-throw-branches
    
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating code coverage report"
  )
  
  # Make coverage target depend on unit_tests
  add_dependencies(coverage unit_tests)
  
  message(STATUS "Code coverage enabled. Use 'make coverage' to generate reports.")
endif()

add_subdirectory(doctest EXCLUDE_FROM_ALL)
add_subdirectory(unit EXCLUDE_FROM_ALL)
add_subdirectory(regression EXCLUDE_FROM_ALL)
add_subdirectory(bench EXCLUDE_FROM_ALL)

# Disable benchmark testing to avoid Google Test dependency
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Enable testing of the benchmark library." FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Enable building the unit tests which depend on gtest" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Build Release candidates with -Werror." FORCE)

# Temporarily disable Werror for benchmark library compilation
set(OLD_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-Werror" "-Wno-error" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# If using Clang, also ensure benchmark library uses libc++
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

add_subdirectory(benchmark EXCLUDE_FROM_ALL)

# Restore original flags
set(CMAKE_CXX_FLAGS "${OLD_CMAKE_CXX_FLAGS}")

