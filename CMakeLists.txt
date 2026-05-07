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
option(RTLOG_BUILD_EXAMPLES "Build example clients" ON)
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

if(RTLOG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(CMakePackageConfigHelpers)

install(EXPORT rt-logger-targets
    FILE rt-logger-targets.cmake
    NAMESPACE rtlog::
    DESTINATION lib/cmake/rt-logger
)

configure_package_config_file(
    cmake/rt-logger-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rt-logger-config.cmake
    INSTALL_DESTINATION lib/cmake/rt-logger
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rt-logger-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/rt-logger-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/rt-logger-config-version.cmake
    DESTINATION lib/cmake/rt-logger
)

file(GLOB_RECURSE ALL_FORMATTABLE_FILES
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.h"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/benchmarks/*.h"
    "${CMAKE_SOURCE_DIR}/benchmarks/*.cpp"
    "${CMAKE_SOURCE_DIR}/examples/*.h"
    "${CMAKE_SOURCE_DIR}/examples/*.cpp"
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
