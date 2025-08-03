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
  include(CodeCoverage)
  append_coverage_compiler_flags()

  set(COVERAGE_EXCLUDES
    "doctest/*"
    "unit/*"
    "regression/*"
    "build/*"
    "build.debug/*"
    "build_debug/*"
    "cmake-build-*"
    "/usr/*")

  set(GCOVR_ADDITIONAL_ARGS
    "--print-summary"
  )

  setup_target_for_coverage_gcovr_html(
    NAME coverage
    EXECUTABLE test/memory_test
  )
endif()

#add_subdirectory(unit EXCLUDE_FROM_ALL)
add_subdirectory(regression EXCLUDE_FROM_ALL)

