cmake_minimum_required(VERSION 3.20)
project(rt-logger VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(tests)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND find
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/tests
            -name "*.cpp" -o -name "*.h" -o -name "*.hpp"
            | xargs ${CLANG_FORMAT_EXE} -i
        COMMENT "Running clang-format..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
