cmake_minimum_required(VERSION 3.20)
project(rt-logger VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RTLOG_BUILD_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(RTLOG_BUILD_SANITIZERS)
    add_library(rt-logger-sanitizers INTERFACE)
    target_compile_options(rt-logger-sanitizers INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(rt-logger-sanitizers INTERFACE
        -fsanitize=address,undefined
    )
endif()

option(RTLOG_BUILD_BENCHMARKS "Build micro-benchmarks" ON)
option(RTLOG_BUILD_TESTS "Build unit tests" ON)

if(RTLOG_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(include)
add_subdirectory(src)

if(RTLOG_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(RTLOG_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

file(GLOB_RECURSE ALL_FORMATTABLE_FILES
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.h"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/benchmarks/*.h"
    "${CMAKE_SOURCE_DIR}/benchmarks/*.cpp"
)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_FORMATTABLE_FILES}
        COMMENT "Running clang-format..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy-19 clang-tidy-18 clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${ALL_FORMATTABLE_FILES}
        COMMENT "Running clang-tidy..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
