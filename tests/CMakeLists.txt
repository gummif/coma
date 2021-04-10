
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

function(coma_add_test TESTNAME)
  add_executable(test_${TESTNAME} ${TESTNAME}.t.cpp)
  target_include_directories(test_${TESTNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(test_${TESTNAME} PRIVATE coma Catch2::Catch2WithMain ${CMAKE_THREAD_LIBS_INIT})
  target_compile_options(test_${TESTNAME} PRIVATE
    # TODO fix this include
    -I/home/gummi/boost/boost_1_75_0
    -fcoroutines
    -fsanitize=address
    -fno-omit-frame-pointer
    -Wall -Wextra -Winit-self -Wreorder)
  #target_compile_definitions(mytest PRIVATE
  #  ASIO_HAS_CO_AWAIT
  #  ASIO_HAS_STD_COROUTINE
  #  ASIO_NO_DEPRECATED
  #  ASIO_NO_TS_EXECUTORS)
  target_link_options(test_${TESTNAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  add_test(NAME ${TESTNAME} COMMAND test_${TESTNAME})
endfunction()

coma_add_test(acquire_guard)
coma_add_test(unique_acquire_guard)
coma_add_test(async_cond_var)
coma_add_test(async_timed_cond_var)
coma_add_test(stranded)
coma_add_test(co_lift)
