# rt-logger — Build, Test, and Install Guide

This document covers everything needed to build, test, benchmark, and install rt-logger from source. It includes prerequisites, standard build commands, sanitizer configurations, static analysis, example clients, and packaging instructions.

> **Quick start:** Install prerequisites → configure with CMake → build → run tests → done.
> For most users, the [Build](#build) section below is all you need.

## Required Packages (Ubuntu 24.04)

```bash
sudo apt-get install -y clang-19 clang-tidy-19 clang-format cmake
```

## Build

The project requires **clang++-19** as the compiler for proper C++23 support
(`std::expected`, `std::jthread`, `std::hardware_destructive_interference_size`).

Clang++ uses the system libstdc++ (from GCC 13+) automatically.

```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19
cmake --build build
ctest --test-dir build
```

## Sanitizers

Enable ASan + UBSan:

```bash
cmake -B build-san -S . -DCMAKE_CXX_COMPILER=clang++-19 -DRTLOG_BUILD_SANITIZERS=ON
cmake --build build-san
ctest --test-dir build-san
```

TSan must be built separately (mutually exclusive with ASan):

```bash
cmake -B build-tsan -S . -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

## clang-tidy

Requires `clang-tidy-19` and a clang-configured build (for `compile_commands.json`).

```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19
cmake --build build --target tidy
```

## clang-format

```bash
cmake --build build --target format
```

## Benchmarks

Enable Google Benchmark (v1.9.1, fetched automatically):

```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19 -DRTLOG_BUILD_BENCHMARKS=ON
cmake --build build --target run-benchmarks
```

All benchmark functions have inline comments describing what they measure:

| Benchmark | What It Tests |
|-----------|--------------|
| `BM_RingTryPushPop` | Single-threaded CAS push + CAS pop cycle |
| `BM_RingTryPushContended` | Multi-threaded CAS push contention (no pops) |
| `BM_RingTryPop` | CAS pop from a pre-filled ring (re-fills after each pop) |
| `BM_RingPushBlocking` | Blocking push with a concurrent consumer draining |
| `BM_ToString` | `LogLevel` → `string_view` lookup (hot path) |
| `BM_SimpleFormat` | Full log formatting with short message + short source path |
| `BM_LongMessageFormat` | Full log formatting with long message + long source path |
| `BM_LoggerSingleThread` | End-to-end `log()` call (timestamp + format + ring push + bg drain) |
| `BM_LoggerMultiThread` | Multi-threaded throughput (N threads × 10k logs each) |
| `BM_LoggerFiltered` | Level-filter rejection path (no ring interaction) |

> **Note:** Benchmarks are built in Debug mode by default. For meaningful numbers, configure with `-DCMAKE_BUILD_TYPE=Release`.

## Examples

Example clients demonstrate real-world usage:

```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19
cmake --build build --target simple_client
cmake --build build --target broadcast_client
cmake --build build --target stress_client
```

| Target | Description |
|--------|-------------|
| `simple_client` | Single-threaded demo with `ConsoleWriter`, level filtering |
| `broadcast_client` | `BroadcastWriter` fans out to `ConsoleWriter` + `FileWriter` |
| `stress_client` | 4 threads × 250K msgs with `FileWriter`, reports throughput |

Control which subcomponents build via CMake options:

| Option | Default | Controls |
|--------|---------|----------|
| `RTLOG_BUILD_TESTS` | `ON` | Unit tests + `enable_testing()` |
| `RTLOG_BUILD_BENCHMARKS` | `ON` | Google Benchmark micro-benchmarks |
| `RTLOG_BUILD_EXAMPLES` | `ON` | Example client programs |

## Install

```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19 \
    -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build
cmake --install build
```

Installed layout:
```
prefix/
├── include/rt-logger/    # Public headers
├── lib/
│   ├── librt-logger.a    # Static library
│   └── cmake/rt-logger/  # CMake package config
│       ├── rt-logger-config.cmake
│       └── rt-logger-config-version.cmake
```

## Consuming via find_package

```cmake
cmake_minimum_required(VERSION 3.20)
project(my-app LANGUAGES CXX)
find_package(rt-logger REQUIRED)
add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE rtlog::rt-logger)
```

Build:
```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/rt-logger/install
cmake --build build
```

## Coverage (GCC-only)

Requires GCC 13+ with gcov:

```bash
cmake -B build-gcov -S . -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage" \
    -DCMAKE_EXE_LINKER_FLAGS="-lgcov"
cmake --build build-gcov
ctest --test-dir build-gcov
lcov --capture --directory build-gcov --output-file coverage.info --ignore-errors mismatch
lcov --remove coverage.info '*/tests/*' '*/_deps/*' '/usr/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage-report --title "rt-logger"
```
