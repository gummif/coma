
include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.4
)
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
  FetchContent_Populate(catch2)
  add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
endif()

find_package(Threads)

# tests
add_executable(tests
  main.t.cpp
  acquire_guard.t.cpp
  unique_acquire_guard.t.cpp
  )
target_link_libraries(tests PRIVATE coma Catch2::Catch2 ${CMAKE_THREAD_LIBS_INIT})
set_property(TARGET tests PROPERTY CXX_STANDARD 20) # TODO do not set
target_compile_options(tests PRIVATE
  -fcoroutines
  -fsanitize=address
  -fno-omit-frame-pointer
  -Wall -Wextra -Winit-self -Wreorder)
target_link_options(tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
add_test(NAME tests COMMAND tests)

# example code TODO cleanup

if(false)
  add_executable(
      mytest
      mytest.cpp
  )
  target_link_libraries(
      mytest
      PRIVATE coma ${CMAKE_THREAD_LIBS_INIT}
  )
  # need to manually build and install libc++ to use clang with coroutines
  target_compile_options(mytest PRIVATE
      -fcoroutines
      -fsanitize=address
      -fno-omit-frame-pointer
      -I/home/gummi/forr/asio/asio/include
      -Wall -Wextra -Winit-self -Wreorder)
  target_link_options(mytest PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  target_compile_definitions(mytest PRIVATE
      ASIO_HAS_CO_AWAIT
      ASIO_HAS_STD_COROUTINE
      ASIO_NO_DEPRECATED
      ASIO_NO_TS_EXECUTORS)
  set_property(TARGET mytest PROPERTY CXX_STANDARD 20)

endif()

# gcc --std=c++20 -fcoroutines -I../../../asio/asio/include -DASIO_HAS_CO_AWAIT -DASIO_HAS_STD_COROUTINE -Wall -Wextra co_timer.cpp -lstdc++ -pthread -lm -lrt