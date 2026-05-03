cmake_minimum_required(VERSION 3.20)
project(rt-logger VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(tests)
