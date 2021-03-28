cmake_minimum_required(VERSION 3.0.0)

project(coma VERSION 0.0.0)

add_library(coma INTERFACE)
target_include_directories(coma INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# need to manually build and install libc++ to use clang with coroutines
target_compile_options(coma INTERFACE
    -fcoroutines
    -fsanitize=address
    -fno-omit-frame-pointer
    -I/home/gummi/forr/asio/asio/include
    -Wall -Wextra -Winit-self -Wreorder)
target_link_options(coma INTERFACE -fsanitize=address -fno-omit-frame-pointer)
target_compile_definitions(coma INTERFACE
    ASIO_HAS_CO_AWAIT
    ASIO_HAS_STD_COROUTINE
    ASIO_NO_DEPRECATED
    ASIO_NO_TS_EXECUTORS)

enable_testing()
add_subdirectory(tests)
