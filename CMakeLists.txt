cmake_minimum_required(VERSION 3.20)

project(divider VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(divider_core
    src/cli.cpp
    src/e_series.cpp
    src/machine_output.cpp
    src/runner.cpp
    src/search.cpp
    src/si.cpp
    src/table.cpp
)

target_include_directories(divider_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(divider src/main.cpp)
target_link_libraries(divider PRIVATE divider_core)
set_target_properties(divider PROPERTIES OUTPUT_NAME "divider")

enable_testing()

add_executable(divider_tests tests/unit_tests.cpp)
target_link_libraries(divider_tests PRIVATE divider_core)

add_test(NAME divider_unit_tests COMMAND divider_tests)
