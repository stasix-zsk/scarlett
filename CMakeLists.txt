cmake_minimum_required(VERSION 3.16)
project(ScarlettVM VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Runtime + codec library
add_library(scarlett_runtime STATIC
    src/common/Codec.cpp
    src/frontend/Parser.cpp
    src/frontend/Lower.cpp
    src/backend/Backend.cpp
    src/runtime/Runtime.cpp
)
target_include_directories(scarlett_runtime PUBLIC include)

# Toolchain CLI
add_executable(scarlett
    src/cli/main.cpp
)
target_link_libraries(scarlett PRIVATE scarlett_runtime)

# Example driver (optional) - zbudowany, jesli demo.protected.cpp zostalo
# juz wygenerowane przez scarlett.exe --outdir out
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/out/demo.protected.cpp)
    add_executable(demo_protected out/demo.protected.cpp)
    target_link_libraries(demo_protected PRIVATE scarlett_runtime)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/out/more.protected.cpp)
    add_executable(more_protected out/more.protected.cpp)
    target_link_libraries(more_protected PRIVATE scarlett_runtime)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/out/strings.protected.cpp)
    add_executable(strings_protected out/strings.protected.cpp)
    target_link_libraries(strings_protected PRIVATE scarlett_runtime)
endif()

# Tests
enable_testing()
add_executable(scarlett_tests tests/test_vm.cpp)
target_link_libraries(scarlett_tests PRIVATE scarlett_runtime)
add_test(NAME scarlett_tests COMMAND scarlett_tests)

add_executable(scarlett_diff_tests tests/test_diff.cpp)
target_link_libraries(scarlett_diff_tests PRIVATE scarlett_runtime)
add_test(NAME scarlett_diff_tests COMMAND scarlett_diff_tests)

add_executable(scarlett_fuzz_tests tests/test_fuzz.cpp)
target_link_libraries(scarlett_fuzz_tests PRIVATE scarlett_runtime)
add_test(NAME scarlett_fuzz_tests COMMAND scarlett_fuzz_tests)

add_executable(scarlett_for_tests tests/test_for.cpp)
target_link_libraries(scarlett_for_tests PRIVATE scarlett_runtime)
add_test(NAME scarlett_for_tests COMMAND scarlett_for_tests)

add_executable(scarlett_string_tests tests/test_string.cpp)
target_link_libraries(scarlett_string_tests PRIVATE scarlett_runtime)
add_test(NAME scarlett_string_tests COMMAND scarlett_string_tests)

add_executable(scarlett_control_tests tests/test_control.cpp)
target_link_libraries(scarlett_control_tests PRIVATE scarlett_runtime)
add_test(NAME scarlett_control_tests COMMAND scarlett_control_tests)
