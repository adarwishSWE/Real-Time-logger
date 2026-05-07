include(FetchContent)

FetchContent_Declare(
    benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(rt-logger-benchmarks)

target_sources(rt-logger-benchmarks PRIVATE
    bench_main.cpp
    bench_ring.cpp
    bench_format.cpp
    bench_logger.cpp
)

target_compile_features(rt-logger-benchmarks PRIVATE cxx_std_23)

target_link_libraries(rt-logger-benchmarks PRIVATE
    rt-logger
    benchmark::benchmark
)

if(TARGET rt-logger-sanitizers)
    target_link_libraries(rt-logger-benchmarks PRIVATE rt-logger-sanitizers)
endif()

add_custom_target(run-benchmarks
    COMMAND rt-logger-benchmarks
    COMMENT "Running micro-benchmarks..."
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
