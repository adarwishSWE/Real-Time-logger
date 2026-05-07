include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_subdirectory(mocks)

add_executable(rt-logger-tests)
target_sources(rt-logger-tests PRIVATE
    log_level_test.cpp
    log_entry_test.cpp
    mpsc_ring_test.cpp
    file_writer_test.cpp
    broadcast_writer_test.cpp
    logger_test.cpp
)

target_compile_features(rt-logger-tests PRIVATE cxx_std_23)
target_link_libraries(rt-logger-tests PRIVATE
    rt-logger
    rt-logger-mocks
    GTest::gtest_main
    GTest::gmock
)

if(TARGET rt-logger-sanitizers)
    target_link_libraries(rt-logger-tests PRIVATE rt-logger-sanitizers)
endif()

gtest_discover_tests(rt-logger-tests)
