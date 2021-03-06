
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
if (COMA_TESTS_BOOST_INC_DIR STREQUAL "")
  # no explicit path set, default to find_package
  find_package(Boost)
  SET(COMA_TESTS_BOOST_INC_DIR ${Boost_INCLUDE_DIR})
endif()
message("Boost include dir: " ${COMA_TESTS_BOOST_INC_DIR})

function(coma_add_test TESTNAME)
  add_executable(test_${TESTNAME} ${TESTNAME}.t.cpp)
  target_include_directories(test_${TESTNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(test_${TESTNAME} PRIVATE coma Catch2::Catch2WithMain ${CMAKE_THREAD_LIBS_INIT})
  target_compile_options(test_${TESTNAME} PRIVATE -I${COMA_TESTS_BOOST_INC_DIR})
  target_compile_definitions(test_${TESTNAME} PRIVATE
    #BOOST_ASIO_HAS_CO_AWAIT
    #BOOST_ASIO_HAS_STD_COROUTINE
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_NO_TS_EXECUTORS)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_${TESTNAME} PRIVATE -Wall -Wextra -Winit-self -Wreorder)
  else()
    target_compile_definitions(test_${TESTNAME} PRIVATE
      _WIN32_WINNT=0x0601
      BOOST_ASIO_HAS_STD_CHRONO
      BOOST_ASIO_DISABLE_BOOST_REGEX
      BOOST_DATE_TIME_NO_LIB
      BOOST_THREAD_NO_LIB
      BOOST_REGEX_NO_LIB
      BOOST_ALL_NO_LIB)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0 OR
     CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.0)
    target_compile_options(test_${TESTNAME} PRIVATE
      -fsanitize=address
      #-fsanitize=undefined
      -fno-omit-frame-pointer)
    target_link_options(test_${TESTNAME} PRIVATE
      -fsanitize=address
      #-fsanitize=undefined
      -fno-omit-frame-pointer)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.2)
    target_compile_options(test_${TESTNAME} PRIVATE -fcoroutines)
    target_compile_definitions(test_${TESTNAME} PRIVATE COMA_ENABLE_COROUTINE_TESTS)
  endif()
  add_test(NAME ${TESTNAME} COMMAND test_${TESTNAME})
endfunction()

coma_add_test(acquire_guard)
coma_add_test(unique_acquire_guard)
coma_add_test(async_semaphore)
coma_add_test(async_cond_var)
coma_add_test(async_cond_var_timed)
coma_add_test(stranded)
coma_add_test(co_lift)
