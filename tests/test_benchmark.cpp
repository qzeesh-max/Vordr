// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/vordr.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <iomanip>
#include <numeric>
#include <cstdio>

#include <atomic>

namespace Benchmark {

// A dummy function simulating an expensive calculation
std::atomic<int> g_dummy_sink{0};
int heavy_computation() {
    return ++g_dummy_sink;
}

// Simulating old-school uncached string concatenation + lookup approach
vordr::LogLevel legacy_uncached_lookup(const vordr::Config& cfg,
                                      std::string_view file,
                                      std::string_view class_name,
                                      std::string_view method_name)
{
    // Emulates doing string copying / concatenation on every check
    std::string qualified;
    if (!class_name.empty() && !method_name.empty()) {
        qualified.reserve(class_name.size() + 2 + method_name.size());
        qualified.append(class_name).append("::").append(method_name);
    }
    return cfg.get_effective_level(file, class_name, method_name);
}

void print_result(std::string_view name, size_t iterations, double duration_ms) {
    double duration_ns = duration_ms * 1e6;
    double ns_per_op = duration_ns / static_cast<double>(iterations);
    double ops_per_sec = (static_cast<double>(iterations) / duration_ms) * 1000.0;
    double mops_per_sec = ops_per_sec / 1e6;

    std::cout << std::left << std::setw(42) << name
              << " | " << std::right << std::setw(10) << iterations
              << " | " << std::setw(9) << std::fixed << std::setprecision(2) << duration_ms << " ms"
              << " | " << std::setw(8) << std::setprecision(2) << ns_per_op << " ns/op"
              << " | " << std::setw(7) << std::setprecision(2) << mops_per_sec << " Mops/s\n";
}

template <typename T>
inline void do_not_optimize(const T& val) {
    asm volatile("" : : "r,m"(val) : "memory");
}

} // namespace Benchmark


int main() {
    std::cout << "\n========================================================================================\n";
    std::cout << "                 VORDR C++26 LOGGER PERFORMANCE & PROFILING BENCHMARK                  \n";
    std::cout << "========================================================================================\n\n";

    auto& logger = vordr::Logger::instance();
    logger.reset();

    const std::string log_file = "/tmp/vordr_bench.log";
    std::remove(log_file.c_str());

    vordr::Config cfg;
    cfg.set_sink_path(log_file);
    cfg.set_sink_max_size(256 * 1024 * 1024); // 256 MB buffer
    cfg.set_central_level(vordr::LogLevel::INFO);
    cfg.set_file_level("service.cpp", vordr::LogLevel::DEBUG);
    cfg.set_class_level("App::PaymentService", vordr::LogLevel::WARN);
    cfg.set_method_level("App::PaymentService::process", vordr::LogLevel::TRACE);
    logger.init(std::move(cfg));

    constexpr size_t ITERATIONS_GATED = 10'000'000; // 10 million iterations
    constexpr size_t ITERATIONS_ENABLED = 1'000'000; // 1 million logs

    std::cout << std::left << std::setw(42) << "Benchmark Scenario"
              << " | " << std::right << std::setw(10) << "Iterations"
              << " | " << std::setw(12) << "Total Time"
              << " | " << std::setw(14) << "Latency"
              << " | " << std::setw(14) << "Throughput\n";
    std::cout << "-------------------------------------------+------------+--------------+----------------+---------------\n";

    // -------------------------------------------------------------
    // Benchmark 1: Uncached String Lookup (Simulated Legacy Approach)
    // -------------------------------------------------------------
    double legacy_ns_per_op = 0.0;
    {
        constexpr size_t RUNS = 1'000'000;
        auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < RUNS; ++i) {
            auto lvl = Benchmark::legacy_uncached_lookup(logger.config(), "service.cpp", "App::PaymentService", "process");
            Benchmark::do_not_optimize(lvl);
        }
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        legacy_ns_per_op = (ms * 1e6) / RUNS;
        Benchmark::print_result("1. Legacy Uncached String Lookup", RUNS, ms);
    }

    // -------------------------------------------------------------
    // Benchmark 2: Zero-Allocation Lookup (Uncached Path)
    // -------------------------------------------------------------
    double zero_alloc_ns_per_op = 0.0;
    {
        constexpr size_t RUNS = 1'000'000;
        vordr::ReflectedContext ctx{
            .method_name = "process",
            .class_name = "App::PaymentService",
            .file_name = "service.cpp",
            .line_number = 100
        };
        auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < RUNS; ++i) {
            auto lvl = logger.get_effective_level_uncached(ctx);
            Benchmark::do_not_optimize(lvl);
        }
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        zero_alloc_ns_per_op = (ms * 1e6) / RUNS;
        Benchmark::print_result("2. Zero-Alloc StringView Lookup", RUNS, ms);
    }

    // -------------------------------------------------------------
    // Benchmark 3: C++26 Reflection CallSiteStorage Fast Path
    // -------------------------------------------------------------
    double fast_path_ns_per_op = 0.0;
    {
        constexpr auto ctx = std::meta::access_context::current();
        constexpr vordr::ReflectedContext ref_ctx = vordr::detail::get_context<ctx.scope()>(__FILE__, __LINE__);
        constexpr uint64_t site_id = vordr::detail::compute_call_site_id(ref_ctx);

        // Warm up call site cache
        vordr::detail::CallSiteStorage<site_id>::is_enabled(vordr::LogLevel::DEBUG, ref_ctx);

        auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < ITERATIONS_GATED; ++i) {
            bool enabled = vordr::detail::CallSiteStorage<site_id>::is_enabled(vordr::LogLevel::DEBUG, ref_ctx);
            Benchmark::do_not_optimize(enabled);
        }
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        fast_path_ns_per_op = (ms * 1e6) / ITERATIONS_GATED;
        Benchmark::print_result("3. C++26 CallSiteStorage (Fast Path)", ITERATIONS_GATED, ms);
    }

    // -------------------------------------------------------------
    // Benchmark 4: Full Macro Gating (Disabled Log Statement with Lazy Eval)
    // -------------------------------------------------------------
    {
        auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < ITERATIONS_GATED; ++i) {
            // Disabled log statement: TRACE is disabled, heavy_computation() is skipped
            VORDR_TRACE << "This is disabled: " << Benchmark::heavy_computation();
        }
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        Benchmark::print_result("4. Full Macro Gating (VORDR_TRACE)", ITERATIONS_GATED, ms);
    }

    // -------------------------------------------------------------
    // Benchmark 5: Enabled Logging through Macro into Mmap File
    // -------------------------------------------------------------
    size_t bytes_before = logger.sink()->bytes_written();
    double write_ms = 0.0;
    {
        auto start = std::chrono::steady_clock::now();
        for (size_t i = 0; i < ITERATIONS_ENABLED; ++i) {
            VORDR_INFO << "Transaction processed id=" << i << " status=SUCCESS";
        }
        logger.flush(false);
        auto end = std::chrono::steady_clock::now();
        write_ms = std::chrono::duration<double, std::milli>(end - start).count();
        Benchmark::print_result("5. Enabled Macro Log -> Mmap File", ITERATIONS_ENABLED, write_ms);
    }
    size_t bytes_after = logger.sink()->bytes_written();
    double total_mb = static_cast<double>(bytes_after - bytes_before) / (1024.0 * 1024.0);
    double throughput_mb_s = (total_mb / (write_ms / 1000.0));

    // -------------------------------------------------------------
    // Benchmark 6: Multi-Threaded Lock-Free Concurrent Logging
    // -------------------------------------------------------------
    constexpr int NUM_THREADS = 8;
    constexpr size_t MSGS_PER_THREAD = 100'000;
    size_t mt_bytes_before = logger.sink()->bytes_written();
    double mt_ms = 0.0;
    {
        std::vector<std::thread> threads;
        threads.reserve(NUM_THREADS);

        auto start = std::chrono::steady_clock::now();
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([t]() {
                for (size_t i = 0; i < MSGS_PER_THREAD; ++i) {
                    VORDR_INFO << "Thread " << t << " event " << i;
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }
        logger.flush(false);
        auto end = std::chrono::steady_clock::now();
        mt_ms = std::chrono::duration<double, std::milli>(end - start).count();
        Benchmark::print_result("6. Lock-Free MT Logging (8 Threads)", NUM_THREADS * MSGS_PER_THREAD, mt_ms);
    }
    size_t mt_bytes_after = logger.sink()->bytes_written();
    double mt_mb = static_cast<double>(mt_bytes_after - mt_bytes_before) / (1024.0 * 1024.0);
    double mt_throughput_mb_s = (mt_mb / (mt_ms / 1000.0));

    std::cout << "-------------------------------------------+------------+--------------+----------------+---------------\n\n";

    // -------------------------------------------------------------
    // Profiling Summary & Speedup Analysis
    // -------------------------------------------------------------
    std::cout << "========================================================================================\n";
    std::cout << "                              PROFILING ANALYSIS & SPEEDUP                              \n";
    std::cout << "========================================================================================\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << " [1] Level Checking Speedup:\n";
    std::cout << "     - Legacy string lookup latency         : " << legacy_ns_per_op << " ns/op\n";
    std::cout << "     - Zero-alloc string_view lookup        : " << zero_alloc_ns_per_op << " ns/op ("
              << (legacy_ns_per_op / zero_alloc_ns_per_op) << "x speedup)\n";
    std::cout << "     - C++26 CallSiteStorage fast-path      : " << fast_path_ns_per_op << " ns/op ("
              << (legacy_ns_per_op / fast_path_ns_per_op) << "x speedup over legacy!)\n\n";

    std::cout << " [2] Mmap Sink Disk Throughput:\n";
    std::cout << "     - Single-threaded write rate           : " << throughput_mb_s << " MB/s ("
              << (static_cast<double>(ITERATIONS_ENABLED) / write_ms * 1000.0 / 1e6) << " Mops/s)\n";
    std::cout << "     - 8-Thread lock-free write rate        : " << mt_throughput_mb_s << " MB/s ("
              << (static_cast<double>(NUM_THREADS * MSGS_PER_THREAD) / mt_ms * 1000.0 / 1e6) << " Mops/s)\n";
    std::cout << "========================================================================================\n\n";

    std::remove(log_file.c_str());
    return 0;
}
