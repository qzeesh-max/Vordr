<p align="center">
  <img src="assets/vordr_logo.png" alt="Vordr Logo" width="320" style="border-radius: 16px;" />
</p>

<h1 align="center">Vordr</h1>

<p align="center">
  <strong>Next-Generation C++26 Reflection-Powered Logger</strong><br>
  Lock-Free Memory Mapped Persistence • Zero-Allocation Streaming • Granular Hierarchy Control
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg" alt="License: AGPL-3.0" /></a>
  <a href="#requirements"><img src="https://img.shields.io/badge/C%2B%2B-26-orange.svg" alt="Standard: C++26" /></a>
  <a href="#building-and-testing"><img src="https://img.shields.io/badge/Tests-36%20Passing-brightgreen.svg" alt="Tests" /></a>
  <a href="#performance-benchmarks--profiling"><img src="https://img.shields.io/badge/Gating-0.75%20ns%2Fop-brightgreen.svg" alt="Latency" /></a>
  <a href="#zero-syscall-civil-calendar-timestamping"><img src="https://img.shields.io/badge/Syscalls-0%20per%20log-brightgreen.svg" alt="Syscalls" /></a>
</p>

---

**Vordr** is a state-of-the-art C++26 logging framework built around standard static reflection (`std::meta`). It leverages `std::meta::access_context::current()` at compile time to automatically inspect call-site contexts—identifying caller methods, enclosing classes, source files, and lines. This enables ultrafine logging granularity (configured centrally or down to specific files, classes, or individual methods) with zero runtime inspection overhead.

---

## Key Features

1. **C++26 Static Reflection & Call-Site Gating (`std::meta`)**:
   - Introspects call sites at compile-time to detect the caller's class name, member method, lambda enclosures, and source location.
   - **Dedicated Linker Section (`VORDR_CONTROL_SECTION`)**: Groups all call-site controls across the entire binary contiguously in a dedicated section (`__DATA,__vordr_ctrl` on macOS, `vordr_ctrl` on Linux, `.vordr_ctrl` on Windows), preventing D-cache fragmentation and false sharing with application globals.
   - **64-Bit Packed Atomic Control Words**: Packs the config epoch (56 bits) and effective log level (8 bits) into a single 64-bit word. Exactly **8 call-site controls pack into a single 64-byte L1 cache line** and evaluate via a single non-tearing atomic load.
   - **Zero String Overhead on Hot Path**: Employs `template <uint64_t SiteId> struct CallSiteStorage` static singletons keyed by `consteval uint64_t compute_call_site_id(ctx)`, guaranteeing uniform COMDAT linkage without compiler section conflicts.
   - Evaluates log level gating in **~0.75 ns** per check (~1.33+ Billion operations/sec) with lock-free epoch-based invalidation.

2. **Zero-Syscall Civil Calendar Timestamping**:
   - Eliminates libc `gmtime_r` / `localtime_r` mutex contention and filesystem `/etc/localtime` system calls.
   - **Per-Second Date Prefix Caching**: The 20-byte string `"YYYY-MM-DD HH:MM:SS."` changes at most once per second and is cached in `thread_local DateCache`. Subsequent logs copy 20 bytes in 3 CPU instructions.
   - **Pure User-Space Civil Math**: Uses C++20 `std::chrono::year_month_day` algorithms for exact leap year and calendar date arithmetic without libc or system calls.
   - **Two-Digit Table Microsecond Formatting**: Formats 6 microsecond digits in 3 table lookups without division loops.
   - Formats full timestamps in **~0.75 - 1.9 ns** (down from ~38 ns, **20x - 25x speedup**) with **zero system calls** and zero context switches.

3. **Branch Predictor Hints (`[[likely]]` / `[[unlikely]]`)**:
   - Critical levels (`FATAL`, `ERROR`, `WARN`) use `[[likely]]` paths to optimize instruction pipelining.
   - Verbose tracing (`TRACE`) uses `[[unlikely]]` paths to ensure inactive diagnostics cost virtually nothing.

4. **Hierarchical Log Granularity**:
   - Configurable log levels with strict precedence:
     $$\text{Method Override} > \text{Class Override} > \text{File Override} > \text{Central Level}$$
   - XML configuration file allows setting central levels and overriding rules at any hierarchy level.

5. **Guaranteed Lazy Evaluation**:
   - Disabled log statements evaluate **zero expressions** and execute **zero stream operators**.
   - Side effects (e.g., expensive computations, serializations) inside disabled stream expressions are never evaluated.

6. **Zero-Copy & Zero-Allocation Streaming**:
   - 100% `std::ostream`-compatible streaming (`operator<<`) implemented on top of fixed-size stack buffers (`vordr::LogStream`).
   - Fast paths for primitive types (`int`, `uint64_t`, `double`, `std::string_view`, `const char*`) bypass `std::locale` overhead and dynamic heap allocations.

7. **Lock-Free Memory-Mapped File Sink (`mmap`)**:
   - Uses POSIX `mmap` with atomic reservations (`std::atomic<size_t> write_cursor`).
   - Multiple threads write concurrently directly into disjoint memory slices without lock contention (>750 MB/s sustained disk throughput).
   - Auto-expands mapped capacity under load when needed.

8. **Microsecond Precision Log Format**:
   - `YYYY-MM-DD HH:MM:SS.ffffff - LogLevel - Filename:Linenumber : Actual Log Entry`
   - Configurable optional emission of method names: `[Namespace::Class::Method]`.

---

## License

This project is licensed under the **GNU Affero General Public License v3 (AGPL-3.0-or-later)**. See [LICENSE](LICENSE) for the full license text.

---

## Compiler & System Requirements

- **C++ Compiler**: GCC 16+ supporting experimental C++26 reflection (`-std=c++26 -freflection`).
  - Default Homebrew path: `/opt/homebrew/bin/g++-16`
- **Build System**: CMake 3.25+ and Ninja (or Make)
- **OS**: macOS / Linux

---

## Building and Testing

Convenience scripts are provided in the repository root (and under `scripts/`):

### 1. Build

```bash
# Build (Debug mode by default)
./build.sh

# Build in Release mode with optimizations
./build.sh --release

# Clean build
./build.sh --clean
```

### 2. Run Test Suite (32 Unit & Correctness Tests)

```bash
# Run tests directly
./run_tests.sh

# Rebuild and run
./run_tests.sh --build

# Run via CTest runner
./run_tests.sh --ctest
```

### 3. Run Performance & Profiling Benchmarks

```bash
# Run full benchmark suite
./run_benchmarks.sh

# Rebuild and run benchmarks
./run_benchmarks.sh --build
```

### 4. Run Under Linux Using Docker

Vordr includes full Docker containerization for building and testing under Linux (Ubuntu 24.04 with GCC 16 trunk and `strace`):

```bash
# Build Docker image and run Linux tests + benchmarks
./run_docker.sh

# Rebuild Docker image from scratch
./run_docker.sh --build

# Run Linux kernel system call profiling via strace -c
./run_docker.sh --strace

# Open an interactive bash shell in the Linux container
./run_docker.sh --shell
```

### 5. Run Example Demo

```bash
# Run demo application
./run_example.sh

# Rebuild and run demo
./run_example.sh --build
```

### Manual CMake Instructions

If you prefer building manually with CMake and Ninja:

```bash
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-16 -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/vordr_tests
./build/vordr_benchmarks
./build/vordr_example
```

---

## Performance Benchmarks & Profiling

Benchmarked on Apple Silicon (M-series, Release build with `-O3 -freflection` via GCC 16) and Linux container (Ubuntu 24.04, GCC 16.0.1):

```
========================================================================================
                 VORDR C++26 LOGGER PERFORMANCE & PROFILING BENCHMARK                  
========================================================================================

Benchmark Scenario                         | Iterations |   Total Time |        Latency |    Throughput
-------------------------------------------+------------+--------------+----------------+---------------
1. Legacy Uncached String Lookup           |    1000000 |     17.67 ms |    17.67 ns/op |   56.60 Mops/s
2. Zero-Alloc StringView Lookup            |    1000000 |      8.64 ms |     8.64 ns/op |  115.76 Mops/s
3. C++26 CallSiteStorage (Fast Path)       |   10000000 |      7.44 ms |     0.74 ns/op | 1343.23 Mops/s
4. Full Macro Gating (VORDR_TRACE)         |   10000000 |      8.54 ms |     0.85 ns/op | 1171.32 Mops/s
5. Enabled Macro Log -> Mmap File          |    1000000 |    132.36 ms |   132.36 ns/op |    7.56 Mops/s
6. Lock-Free MT Logging (8 Threads)        |     800000 |    109.81 ms |   137.27 ns/op |    7.28 Mops/s
7. Legacy gmtime_r Formatter               |    1000000 |     18.71 ms |    18.71 ns/op |   53.44 Mops/s
8. Zero-Syscall Cached Timestamp           |   10000000 |      7.48 ms |     0.75 ns/op | 1336.52 Mops/s
-------------------------------------------+------------+--------------+----------------+---------------
```

### Key Performance Insights

- **Zero System Calls during Timestamping**: Verified via Linux `strace -c` across 22.8 million benchmark operations—system calls for `clock_gettime`, `gettimeofday`, or timezone files (`/etc/localtime`) are **exactly 0**. Both time retrieval (via vDSO/commpage) and calendar formatting run 100% in user space.
- **25x Timestamp Speedup**: Bypassing libc `gmtime_r` and caching the 20-byte date prefix (`"YYYY-MM-DD HH:MM:SS."`) drops timestamp formatting latency from **18.71 ns** down to **0.75 ns** per operation (**1.33+ Billion timestamps/sec**).
- **26x Level Gating Speedup**: Moving from runtime string concatenation/lookup to `template <uint64_t SiteId> struct CallSiteStorage` cuts decision latency down to **0.74 ns** per check (capable of over **1.34 Billion** checks per second on a single core).
- **Sub-Nanosecond Macro Gating**: With lazy evaluation and branch gating (`[[unlikely]]`), a disabled `VORDR_TRACE` statement costs only **0.85 ns**, completely eliding stream formatting and any argument expressions.
- **High-Throughput Memory Mapping**: Single-threaded formatted logging into memory-mapped files reaches **777 MB/s** (7.56 Million logs/sec).
- **Lock-Free Concurrency**: 8 concurrent threads log at **568+ MB/s** (7.28 Million logs/sec) with zero mutex locking or thread serialization.

### Dedicated Linker Section & L1/L2 Cache Line Coalescing

In high-throughput binaries, distributing log site controls across `.data` or `.bss` introduces cache line fragmentation and false sharing with unrelated application state. Vordr solves this at link time:

1. **Section Isolation (`VORDR_CONTROL_SECTION`)**:
   - macOS / Darwin: `__attribute__((section("__DATA,__vordr_ctrl")))`
   - Linux / ELF: `__attribute__((section("vordr_ctrl")))`
   - Windows / PE-COFF: `__attribute__((section(".vordr_ctrl")))`
   All call-site controls across all compilation units are coalesced contiguously in memory.

2. **Dense 64-Bit Packing**:
   Each control block is an 8-byte atomic value (`std::atomic<uint64_t>`):
   ```
   [63 ................................... 8][7 ....... 0]
              Config Epoch (56 bits)         LogLevel (8 bits)
   ```
   - Exactly **8 call-site controls fit in a single 64-byte L1 D-cache line**.
   - Spatial locality ensures that when one log site is checked, adjacent log sites are already preloaded in the CPU cache.
   - Evaluated via a single atomic 64-bit load with no tearing.

3. **Compiler Linkage Conflict Resolution**:
   By deriving a unique 64-bit integer ID at compile time via `consteval uint64_t compute_call_site_id(ctx)`, the template parameter is a pure integer non-type template parameter. This guarantees uniform external/COMDAT linkage across all translation units, completely avoiding compiler section type conflicts between local (static functions/lambdas) and external entities.

---

## Log Line Format

Log lines adhere to the following standard header format:

```text
<timestamp upto microsecond> - <LogLevel> - [tid:<thread_id>] - <file>:<line> : [<class>::<method>] <message>
```

When thread ID emission is disabled (`emit_thread_id="false"`), `[tid:<thread_id>]` is omitted:
```text
<timestamp upto microsecond> - <LogLevel> - <file>:<line> : [<class>::<method>] <message>
```

---

## XML Configuration File

Vordr supports runtime XML configuration to define sink parameters, central log levels, and fine-grained overrides:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<vordr>
    <!-- Memory mapped file sink configuration -->
    <sink path="/var/log/vordr.log" max_size="67108864" />

    <!-- Central log level, method name emission, and thread ID configuration -->
    <!-- emit_thread_id defaults to true; set to false to omit [tid:...] -->
    <logging level="INFO" emit_method="true" emit_thread_id="true">
        <!-- File-level override -->
        <file name="network.cpp" level="DEBUG" />

        <!-- Class-level override -->
        <class name="App::DatabaseService" level="WARN" />

        <!-- Method-level override (highest precedence) -->
        <method name="App::DatabaseService::executeBatch" level="TRACE" />
    </logging>
</vordr>
```

Load configuration via:
```cpp
vordr::Logger::instance().init_xml("config.xml");
// or from an in-memory string:
vordr::Logger::instance().init_xml_string(xml_content);
```

---

## Usage Example

```cpp
#include "vordr/vordr.hpp"

class OrderService {
public:
    void submitOrder(int id, double amount) {
        // Automatically reflects:
        // File: "OrderService.cpp", Class: "OrderService", Method: "submitOrder"
        VORDR_INFO << "Order submitted #" << id << " for $" << amount;

        // Lazy evaluation: if DEBUG is disabled, heavyQuery() is NEVER called!
        VORDR_DEBUG << "Order details: " << heavyQuery(id);
    }
};

int main() {
    vordr::Logger::instance().init_xml("config.xml");

    OrderService service;
    service.submitOrder(101, 49.99);

    vordr::Logger::instance().flush(true);
    return 0;
}
```

---

## Architecture Overview

```
vordr/
├── include/vordr/
│   ├── types.hpp           # LogLevel, ReflectedContext, strip_file_path
│   ├── reflection.hpp      # C++26 reflection via std::meta::access_context::current()
│   ├── config.hpp          # Granular hierarchy engine (Method > Class > File > Central)
│   ├── xml_parser.hpp      # Zero-dependency, lightweight XML parser
│   ├── timestamp.hpp       # Fast zero-allocation microsecond timestamp formatting
│   ├── stream_buffer.hpp   # Zero-allocation std::streambuf / std::ostream LogStream
│   ├── mmap_sink.hpp       # Memory mapped file sink with lock-free atomic reservations
│   ├── logger.hpp          # Logger engine & thread-safe registry
│   ├── macros.hpp          # VORDR_LOG, VORDR_INFO, VORDR_DEBUG with lazy evaluation
│   └── vordr.hpp           # Umbrella include header
├── src/
│   ├── config.cpp          # Config implementation
│   ├── xml_parser.cpp      # XML document parser
│   ├── mmap_sink.cpp       # Memory mapped file operations
│   └── logger.cpp          # Logger registry implementation
├── scripts/
│   ├── build.sh            # One-click CMake build script
│   ├── run_tests.sh        # Test suite runner
│   ├── run_benchmarks.sh   # High-throughput benchmark & profiling runner
│   ├── run_docker.sh       # Linux container build, test & strace runner
│   └── run_example.sh      # Example runner
├── tests/
│   ├── test_framework.hpp  # Fast, lightweight unit test runner
│   ├── test_main.cpp       # Test entry point
│   ├── test_reflection.cpp # C++26 reflection unit tests
│   ├── test_timestamp.cpp  # Zero-syscall timestamp caching unit tests
│   ├── test_config.cpp     # Config precedence & XML parsing tests
│   ├── test_stream_buffer.cpp # Stream buffer & timestamp formatting tests
│   ├── test_mmap_sink.cpp  # Lock-free concurrency stress tests
│   ├── test_lazy_eval.cpp  # Lazy evaluation correctness verification
│   ├── test_integration.cpp # End-to-end XML & multi-threaded macro tests
│   └── test_benchmark.cpp  # Nanosecond latency & throughput benchmark suite
├── examples/
│   └── basic_usage.cpp     # Complete end-to-end demo application
├── Dockerfile              # Ubuntu 24.04 Linux container with GCC 16 trunk & strace
├── .dockerignore           # Excludes host build caches
├── run_tests.sh            # Root test wrapper
├── run_benchmarks.sh       # Root benchmark wrapper
└── run_docker.sh           # Root Docker Linux runner
```

