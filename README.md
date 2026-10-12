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
  <a href="#building-and-testing"><img src="https://img.shields.io/badge/Tests-26%20Passing-brightgreen.svg" alt="Tests" /></a>
  <a href="#performance-benchmarks--profiling"><img src="https://img.shields.io/badge/Gating-0.81%20ns%2Fop-brightgreen.svg" alt="Latency" /></a>
</p>

---

**Vordr** is a state-of-the-art C++26 logging framework built around standard static reflection (`std::meta`). It leverages `std::meta::access_context::current()` at compile time to automatically inspect call-site contexts—identifying caller methods, enclosing classes, source files, and lines. This enables ultrafine logging granularity (configured centrally or down to specific files, classes, or individual methods) with zero runtime inspection overhead.

---

## Key Features

1. **C++26 Static Reflection & Call-Site Gating (`std::meta`)**:
   - Introspects call sites at compile-time to detect the caller's class name, member method, lambda enclosures, and source location.
   - **Zero String Overhead on Hot Path**: Employs `template <std::meta::info Scope, uint32_t Line> struct CallSiteStorage` static singletons. Moves all string copying, concatenation, and hash lookups completely off the hot path.
   - Evaluates log level gating in **~0.81 ns** per check (~1.23+ Billion operations/sec) using an atomic config epoch for lock-free cache invalidation.

2. **Branch Predictor Hints (`[[likely]]` / `[[unlikely]]`)**:
   - Critical levels (`FATAL`, `ERROR`, `WARN`) use `[[likely]]` paths to optimize instruction pipelining.
   - Verbose tracing (`TRACE`) uses `[[unlikely]]` paths to ensure inactive diagnostics cost virtually nothing.

3. **Hierarchical Log Granularity**:
   - Configurable log levels with strict precedence:
     $$\text{Method Override} > \text{Class Override} > \text{File Override} > \text{Central Level}$$
   - XML configuration file allows setting central levels and overriding rules at any hierarchy level.

4. **Guaranteed Lazy Evaluation**:
   - Disabled log statements evaluate **zero expressions** and execute **zero stream operators**.
   - Side effects (e.g., expensive computations, serializations) inside disabled stream expressions are never evaluated.

5. **Zero-Copy & Zero-Allocation Streaming**:
   - 100% `std::ostream`-compatible streaming (`operator<<`) implemented on top of fixed-size stack buffers (`vordr::LogStream`).
   - Fast paths for primitive types (`int`, `uint64_t`, `double`, `std::string_view`, `const char*`) bypass `std::locale` overhead and dynamic heap allocations.

6. **Lock-Free Memory-Mapped File Sink (`mmap`)**:
   - Uses POSIX `mmap` with atomic reservations (`std::atomic<size_t> write_cursor`).
   - Multiple threads write concurrently directly into disjoint memory slices without lock contention (>600 MB/s sustained disk throughput).
   - Auto-expands mapped capacity under load when needed.

7. **Microsecond Precision Log Format**:
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

### 2. Run Test Suite (26 Unit & Correctness Tests)

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

### 4. Run Example Demo

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

Benchmarked on Apple Silicon (M-series, Release build with `-O3` via GCC 16):

```
========================================================================================
                 VORDR C++26 LOGGER PERFORMANCE & PROFILING BENCHMARK                  
========================================================================================

Benchmark Scenario                         | Iterations |   Total Time |        Latency |    Throughput
-------------------------------------------+------------+--------------+----------------+---------------
1. Legacy Uncached String Lookup           |    1000000 |     23.40 ms |    23.40 ns/op |   42.74 Mops/s
2. Zero-Alloc StringView Lookup            |    1000000 |     10.05 ms |    10.05 ns/op |   99.47 Mops/s
3. C++26 CallSiteStorage (Fast Path)       |   10000000 |      8.08 ms |     0.81 ns/op | 1237.47 Mops/s
4. Full Macro Gating (VORDR_TRACE)         |   10000000 |      8.93 ms |     0.89 ns/op | 1120.07 Mops/s
5. Enabled Macro Log -> Mmap File          |    1000000 |    163.68 ms |   163.68 ns/op |    6.11 Mops/s
6. Lock-Free MT Logging (8 Threads)        |     800000 |    385.57 ms |   481.97 ns/op |    2.07 Mops/s
-------------------------------------------+------------+--------------+----------------+---------------
```

### Key Performance Insights

- **28.95x Gating Speedup**: Moving from runtime string concatenation/lookup to `template <std::meta::info Scope, uint32_t Line> struct CallSiteStorage` cuts decision latency from **23.40 ns** down to **0.81 ns** per check (capable of over **1.23 Billion** checks per second on a single core).
- **Sub-Nanosecond Macro Gating**: With lazy evaluation and branch gating (`[[unlikely]]`), a disabled `VORDR_TRACE` statement costs only **0.89 ns**, completely eliding stream formatting and any argument expressions.
- **High-Throughput Memory Mapping**: Single-threaded formatted logging into memory-mapped files clocks in at **628.61 MB/s** (6.11 Million logs/sec).
- **Lock-Free Concurrency**: 8 concurrent threads log at **162+ MB/s** with zero mutex locking or thread serialization.

---

## XML Configuration File

Vordr supports runtime XML configuration to define sink parameters, central log levels, and fine-grained overrides:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<vordr>
    <!-- Memory mapped file sink configuration -->
    <sink path="/var/log/vordr.log" max_size="67108864" />

    <!-- Central log level and optional method name emission -->
    <logging level="INFO" emit_method="true">
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
│   └── run_example.sh      # Example runner
├── tests/
│   ├── test_framework.hpp  # Fast, lightweight unit test runner
│   ├── test_main.cpp       # Test entry point
│   ├── test_reflection.cpp # C++26 reflection unit tests
│   ├── test_config.cpp     # Config precedence & XML parsing tests
│   ├── test_stream_buffer.cpp # Stream buffer & timestamp formatting tests
│   ├── test_mmap_sink.cpp  # Lock-free concurrency stress tests
│   ├── test_lazy_eval.cpp  # Lazy evaluation correctness verification
│   ├── test_integration.cpp # End-to-end XML & multi-threaded macro tests
│   └── test_benchmark.cpp  # Nanosecond latency & throughput benchmark suite
└── examples/
    └── basic_usage.cpp     # Complete end-to-end demo application
```

