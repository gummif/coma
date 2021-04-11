cmake_minimum_required(VERSION 3.0.0)

project(coma VERSION 0.0.0)

SET(COMA_ENABLE_TESTS "0" CACHE BOOL "Enable testing")
SET(COMA_TESTS_BOOST_INC_DIR "" CACHE STRING "Boost include dir for tests")

add_library(coma INTERFACE)
target_include_directories(coma INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(${COMA_ENABLE_TESTS})
  message("Boost include dir: " ${COMA_TESTS_BOOST_INC_DIR})
  enable_testing()
  add_subdirectory(tests)
endif()
