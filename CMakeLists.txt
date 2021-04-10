cmake_minimum_required(VERSION 3.0.0)

project(coma VERSION 0.0.0)

add_library(coma INTERFACE)
target_include_directories(coma INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

enable_testing()
add_subdirectory(tests)
