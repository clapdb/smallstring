#[[
Licensed under the Apache License, Version 2.0(the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
]]

cmake_minimum_required(VERSION 3.10)

aux_source_directory(. REG_SRC)

add_executable(reg_test EXCLUDE_FROM_ALL ${REG_SRC})

## test flags
list(APPEND TEST_FLAGS
#- DBOOST_STACKTRACE_USE_BACKTRACE
  -DDOCTEST_CONFIG_SUPER_FAST_ASSERTS
  -Wno-deprecated
)

target_compile_options(reg_test PRIVATE ${TEST_FLAGS})

target_link_libraries(reg_test PRIVATE pthread doctest fmt)
