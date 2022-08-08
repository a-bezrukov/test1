cmake_minimum_required(VERSION 3.0)

add_compile_options(-Wall -Wextra -pedantic -Werror)

add_executable(test1 src/main.cpp)

set_property(TARGET test1 PROPERTY CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-pthread")