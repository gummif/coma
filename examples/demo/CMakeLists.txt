cmake_minimum_required(VERSION 3.14)
project(coma-demo)

include(FetchContent)
FetchContent_Declare(coma
  GIT_REPOSITORY https://github.com/gummif/coma.git
  GIT_TAG master
)
FetchContent_MakeAvailable(coma)

find_package(Boost)

add_executable(coma-demo main.cpp)
target_link_libraries(coma-demo coma::coma Boost::headers)
