# rt-logger

**Zero-allocation, bounded-latency logging for real-time C++ systems.**

`rt-logger` is a single-static-library logging backend built for embedded and real-time applications where **predictable producer-side latency matters**. A single `log()` call takes **~500 nanoseconds** when the ring buffer has space — the consumer thread handles all slow I/O in the background.

## The Problem

Traditional logging (`printf`, `std::cout`, `spdlog` with default settings) blocks the calling thread until the message is fully written. In a real-time system, that means:

- A sensor-reading thread suddenly stalls for **milliseconds** because the log file is being flushed
- A motor-control loop misses its deadline because `fprintf` is waiting on disk I/O
- Memory usage grows without bound under burst traffic as the log queue balloons

## How rt-logger Fixes It

```
Producer Threads (real-time)          Consumer Thread (background)
        │                                      │
        ▼                                      ▼
   ┌─────────┐                          ┌─────────────┐
   │ log()   │ ── enqueue ──►           │ try_pop()   │
   │ ~500ns  │    (fast path)           │ format()    │
   └─────────┘                          │ write()     │
      (blocks only                      │ (slow I/O)  │
       if ring full)                    └─────────────┘
```

1. **Fast-path enqueue via CAS**: When the ring has space, producers reserve a slot with a single atomic compare-and-swap — no mutex, no kernel call.
2. **Bounded blocking**: When the ring is full, producers wait on a condition variable. The ring size (e.g., 1024 slots) caps both memory and worst-case stall time.
3. **Background consumer**: A single `std::jthread` drains the ring, formats messages, and writes to pluggable backends. Slow I/O never touches the producer.
4. **Zero-allocation `log()`**: `LogEntry` is a fixed-size POD struct (~288 bytes: `Timestamp` 8B + `LogLevel` 4B + `SourceLoc` 24B + `message[256]` 256B) on the stack — no `new`, no `delete`, no exceptions.

## When to Use rt-logger

| Use rt-logger | Use something else (e.g., spdlog, fmt) |
|---------------|----------------------------------------|
| You need **predictable, sub-µs** log latency when buffer has space | You need maximum throughput and don't care about latency spikes |
| You run on **embedded** or **bare-metal** with `-fno-exceptions` | You want rich formatting, async file sinks, or built-in log rotation |
| You have **multiple real-time threads** logging concurrently | You only log from a single thread |
| You need **bounded memory** — no unbounded queues | You want "fire and forget" with unlimited buffering |

## Key Features

| Feature | Why It Matters |
|---------|---------------|
| **CAS-based MPSC ring** | Fast-path enqueue via atomic compare-and-swap when ring has space |
| **Zero-allocation hot path** | `LogEntry` is a fixed-size POD — no heap, no fragmentation |
| **`std::expected` error handling** | No exceptions — suitable for `-fno-exceptions` embedded builds |
| **Background consumer thread** | Producers never wait for disk, network, or terminal I/O |
| **Runtime level filtering** | Change minimum log level on the fly via `set_level()` |
| **Pluggable writers** | `ConsoleWriter`, `FileWriter`, `BroadcastWriter` — or implement `ILogWriter` |
| **Graceful shutdown** | `shutdown()` drains remaining entries and joins the consumer thread |

## Prerequisites

- **Compiler**: `clang++-19` — required for full C++23 standard library support (`std::expected`, `std::jthread`); pair with libstdc++ from GCC 13+
- **Build system**: CMake ≥ 3.20
- **OS**: Linux (tested on Ubuntu 24.04)

```bash
sudo apt-get install -y clang-19 cmake
```

## Quick Start

```cpp
#include <rt-logger/file_writer.h>
#include <rt-logger/logger.h>
#include <rt-logger/mpsc_ring.h>

auto ring   = std::make_unique<rtlog::MpscRing<1024>>();
auto writer = rtlog::FileWriter::create("app.log");
rtlog::Logger logger{std::move(ring), std::move(writer), rtlog::LogLevel::INFO};

rtlog::SourceLoc loc{"main.cpp", __LINE__, __func__};
logger.log(rtlog::LogLevel::INFO, "System initialized", loc);
logger.log(rtlog::LogLevel::WARN, "Temperature high", loc);
logger.shutdown();
```

> **Note:** `__LINE__` and `__func__` capture the *call site*. In production you'd typically wrap this in a macro:
> ```cpp
> #define LOG(logger, level, msg) \
>     logger.log(level, msg, rtlog::SourceLoc{__FILE__, __LINE__, __func__})
> LOG(logger, rtlog::LogLevel::INFO, "System initialized");
> ```

## Build & Install

```bash
# Configure (release build recommended for production)
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_BUILD_TYPE=Release

# Build library, tests, examples, and benchmarks
cmake --build build

# Run tests
ctest --test-dir build

# Install to system prefix (default: /usr/local)
cmake --install build

# Or install to custom prefix
cmake --install build --prefix /opt/rt-logger
```

See [BUILD.md](BUILD.md) for full details: sanitizers, clang-tidy, coverage, and benchmark builds.

## Consuming via `find_package`

```cmake
cmake_minimum_required(VERSION 3.20)
project(my-app LANGUAGES CXX)
find_package(rt-logger REQUIRED)
add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE rtlog::rt-logger)
```

Configure with the install prefix:
```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=/opt/rt-logger
cmake --build build
```

## API Overview

```cpp
// Submit a log entry (thread-safe, non-blocking when ring has space)
std::expected<void, LoggerError>
log(LogLevel level, std::string_view message, const SourceLoc& source_loc);

// Change minimum log level at runtime (thread-safe)
void set_level(LogLevel level);

// Replace the output writer at runtime (thread-safe)
void set_writer(std::unique_ptr<ILogWriter> writer);

// Graceful shutdown: drain remaining entries, join consumer thread
void shutdown();
```

## Architecture

```
Producer Threads (any number)
         │
         ▼
┌─────────────────┐
│   MpscRing<N>   │  ← CAS enqueue (fast path)
│  (bounded ring) │
└─────────────────┘
         │
         ▼
Consumer Thread (1)
    try_pop() → format_entry() → ILogWriter::write()
         │
         ▼
   ConsoleWriter / FileWriter / BroadcastWriter
```

> **Bounded blocking:** When the ring is full, producers wait on a condition variable until a slot is freed. The ring size `N` directly caps both memory usage and worst-case producer stall time.

## Benchmarks

**Latency is the headline metric.** Throughput is intentionally modest because `FileWriter` is synchronous — the point is that the *producer* never waits for I/O, not that the library outperforms dedicated async loggers like `spdlog` (which can do millions of msg/sec with buffered sinks).

Producer-side latency (Debug build, 4.7GHz x86-64):

| Operation | Latency |
|-----------|---------|
| `log()` end-to-end (ring has space) | **~500 ns** |
| Ring `try_push` (single-threaded CAS) | ~70 ns |
| Ring `try_push` (contended, 4 threads) | ~18 ns amortized |
| Message formatting (`strftime` + `snprintf`) | ~200 ns |

End-to-end throughput (limited by synchronous `FileWriter`, not the ring):

| Scenario | Throughput | Bottleneck |
|----------|-----------|------------|
| FileWriter (sync disk I/O) | 25,874 msg/sec | `std::ostream::write()` (~15 µs/msg) |
| NullWriter (no I/O ceiling) | 41,881 msg/sec | Consumer 1 ms sleep + push mutex |

> **Why the throughput gap?** The ring itself can sustain millions of operations per second — the limit is the *consumer* writing to disk. If you need higher throughput, replace `FileWriter` with a batched or async writer, or use `NullWriter` for in-memory telemetry.

Run the full 14-benchmark suite locally:
```bash
cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++-19
cmake --build build --target run-benchmarks
```

## Examples

| Example | Description |
|---------|-------------|
| [`simple_client`](examples/simple_client.cpp) | Single-threaded demo with level filtering |
| [`stress_client`](examples/stress_client.cpp) | 4-thread burn-in, measures throughput |
| [`broadcast_client`](examples/broadcast_client.cpp) | Fan-out to console + file simultaneously |

Build and run:
```bash
cmake --build build --target simple_client
./build/examples/simple_client
```

## License

MIT — see [LICENSE](LICENSE) for details.
