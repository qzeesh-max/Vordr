# Vordr: High-Performance C++26 Reflection-Powered Logger

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)
[![Standard: C++26](https://img.shields.io/badge/C%2B%2B-26-orange.svg)](#requirements)

**Vordr** is a state-of-the-art C++26 logging framework built around standard static reflection (`std::meta`). It leverages `std::meta::access_context::current()` at compile time to automatically inspect call-site contexts—identifying caller methods, enclosing classes, source files, and lines. This enables ultrafine logging granularity (configured centrally or down to specific files, classes, or individual methods) with zero runtime inspection overhead.

---

## Key Features

1. **C++26 Static Reflection (`std::meta::access_context::current()`)**:
   - Introspects call sites at compile-time to detect the caller's class name, member method, lambda enclosures, and source location.
   - Eliminates manual string tags or boilerplate logger categories.

2. **Hierarchical Log Granularity**:
   - Configurable log levels with strict precedence:
     $$\text{Method Override} > \text{Class Override} > \text{File Override} > \text{Central Level}$$
   - XML configuration file allows setting central levels and overriding rules at any hierarchy level.

3. **Guaranteed Lazy Evaluation**:
   - Disabled log statements evaluate **zero expressions** and execute **zero stream operators**.
   - Side effects (e.g., expensive computations, serializations) inside disabled stream expressions are never evaluated.

4. **Zero-Copy & Zero-Allocation Streaming**:
   - 100% `std::ostream`-compatible streaming (`operator<<`) implemented on top of fixed-size stack buffers (`vordr::LogStream`).
   - Fast paths for primitive types (`int`, `uint64_t`, `double`, `std::string_view`, `const char*`) bypass `std::locale` overhead and dynamic heap allocations.

5. **Lock-Free Memory-Mapped File Sink (`mmap`)**:
   - Uses POSIX `mmap` with atomic reservations (`std::atomic<size_t> write_cursor`).
   - Multiple threads write concurrently directly into disjoint memory slices without lock contention.
   - Auto-expands mapped capacity under load when needed.

6. **Microsecond Precision Log Format**:
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

### 2. Run Test Suite (25 Unit & Correctness Tests)

```bash
# Run tests directly
./run_tests.sh

# Rebuild and run
./run_tests.sh --build

# Run via CTest runner
./run_tests.sh --ctest
```

### 3. Run Example Demo

```bash
# Run demo application
./run_example.sh

# Rebuild and run demo
./run_example.sh --build
```

### Manual CMake Instructions

If you prefer building manually with CMake and Ninja:

```bash
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-16
ninja -C build
./build/vordr_tests
./build/vordr_example
```

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
├── tests/
│   ├── test_framework.hpp  # Fast, lightweight unit test runner
│   ├── test_main.cpp       # Test entry point
│   ├── test_reflection.cpp # C++26 reflection unit tests
│   ├── test_config.cpp     # Config precedence & XML parsing tests
│   ├── test_stream_buffer.cpp # Stream buffer & timestamp formatting tests
│   ├── test_mmap_sink.cpp  # Lock-free concurrency stress tests
│   ├── test_lazy_eval.cpp  # Lazy evaluation correctness verification
│   └── test_integration.cpp # End-to-end XML & multi-threaded macro tests
└── examples/
    └── basic_usage.cpp     # Complete end-to-end demo application
```
