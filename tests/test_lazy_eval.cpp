// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/vordr.hpp"
#include <cstdio>

namespace LazyTest {

static int g_side_effect_count = 0;

int calculate_expensive_value(int val) {
    ++g_side_effect_count;
    return val * 10;
}

class TestWorker {
public:
    void do_debug_log() {
        VORDR_DEBUG << "Result is: " << calculate_expensive_value(5);
    }

    void do_info_log() {
        VORDR_INFO << "Important result: " << calculate_expensive_value(7);
    }

    void do_trace_log() {
        VORDR_TRACE << "Trace result: " << calculate_expensive_value(1);
    }
};

} // namespace LazyTest

TEST_CASE("Lazy Evaluation - Disabled Level Avoids Expression Execution") {
    const std::string log_file = "/tmp/vordr_lazy_test.log";
    std::remove(log_file.c_str());

    auto& logger = vordr::Logger::instance();
    logger.reset();
    vordr::Config cfg;
    cfg.set_sink_path(log_file);
    cfg.set_central_level(vordr::LogLevel::INFO);
    logger.init(std::move(cfg));

    LazyTest::g_side_effect_count = 0;
    LazyTest::TestWorker worker;

    // DEBUG is below central INFO -> should NOT evaluate calculate_expensive_value
    worker.do_debug_log();
    ASSERT_EQ(LazyTest::g_side_effect_count, 0);

    // TRACE is below central INFO -> should NOT evaluate
    worker.do_trace_log();
    ASSERT_EQ(LazyTest::g_side_effect_count, 0);

    // INFO is enabled -> SHOULD evaluate calculate_expensive_value exactly once
    worker.do_info_log();
    ASSERT_EQ(LazyTest::g_side_effect_count, 1);

    logger.flush(true);
    std::string_view content = logger.sink()->written_content();
    ASSERT_TRUE(content.find("Important result: 70") != std::string_view::npos);
    ASSERT_TRUE(content.find("Result is:") == std::string_view::npos);

    std::remove(log_file.c_str());
}

TEST_CASE("Lazy Evaluation - Granular Method Level Activation") {
    const std::string log_file = "/tmp/vordr_lazy_method.log";
    std::remove(log_file.c_str());

    auto& logger = vordr::Logger::instance();
    logger.reset();
    vordr::Config cfg;
    cfg.set_sink_path(log_file);
    cfg.set_central_level(vordr::LogLevel::ERROR); // everything disabled by default

    // Selectively enable TRACE on do_trace_log
    cfg.set_method_level("LazyTest::TestWorker::do_trace_log", vordr::LogLevel::TRACE);
    logger.init(std::move(cfg));

    LazyTest::g_side_effect_count = 0;
    LazyTest::TestWorker worker;

    // do_info_log is at ERROR level, so INFO is disabled -> 0 calls
    worker.do_info_log();
    ASSERT_EQ(LazyTest::g_side_effect_count, 0);

    // do_trace_log has method override for TRACE -> 1 call
    worker.do_trace_log();
    ASSERT_EQ(LazyTest::g_side_effect_count, 1);

    logger.flush(true);
    std::string_view content = logger.sink()->written_content();
    ASSERT_TRUE(content.find("Trace result: 10") != std::string_view::npos);

    std::remove(log_file.c_str());
}

TEST_CASE("Branch Gating - Likely and Unlikely Levels") {
    const std::string log_file = "/tmp/vordr_branch_test.log";
    std::remove(log_file.c_str());

    auto& logger = vordr::Logger::instance();
    logger.reset();
    vordr::Config cfg;
    cfg.set_sink_path(log_file);
    cfg.set_central_level(vordr::LogLevel::WARN);
    logger.init(std::move(cfg));

    LazyTest::g_side_effect_count = 0;

    // TRACE is gated out (unlikely branch taken)
    VORDR_TRACE << "Trace side-effect: " << LazyTest::calculate_expensive_value(1);
    ASSERT_EQ(LazyTest::g_side_effect_count, 0);

    // Generic VORDR_LOG with TRACE
    VORDR_LOG(vordr::LogLevel::TRACE) << "Generic trace side-effect: " << LazyTest::calculate_expensive_value(2);
    ASSERT_EQ(LazyTest::g_side_effect_count, 0);

    // WARN is enabled (likely branch taken)
    VORDR_WARN << "Warn side-effect: " << LazyTest::calculate_expensive_value(3);
    ASSERT_EQ(LazyTest::g_side_effect_count, 1);

    // ERROR is enabled (likely branch taken)
    VORDR_ERROR << "Error side-effect: " << LazyTest::calculate_expensive_value(4);
    ASSERT_EQ(LazyTest::g_side_effect_count, 2);

    // FATAL is enabled (likely branch taken)
    VORDR_FATAL << "Fatal side-effect: " << LazyTest::calculate_expensive_value(5);
    ASSERT_EQ(LazyTest::g_side_effect_count, 3);

    // Generic VORDR_LOG with WARN/ERROR/FATAL
    VORDR_LOG(vordr::LogLevel::WARN) << "Generic warn: " << LazyTest::calculate_expensive_value(6);
    ASSERT_EQ(LazyTest::g_side_effect_count, 4);

    VORDR_LOG(vordr::LogLevel::FATAL) << "Generic fatal: " << LazyTest::calculate_expensive_value(7);
    ASSERT_EQ(LazyTest::g_side_effect_count, 5);

    logger.flush(true);
    std::string_view content = logger.sink()->written_content();
    ASSERT_TRUE(content.find("Warn side-effect: 30") != std::string_view::npos);
    ASSERT_TRUE(content.find("Error side-effect: 40") != std::string_view::npos);
    ASSERT_TRUE(content.find("Fatal side-effect: 50") != std::string_view::npos);
    ASSERT_TRUE(content.find("Generic warn: 60") != std::string_view::npos);
    ASSERT_TRUE(content.find("Generic fatal: 70") != std::string_view::npos);
    ASSERT_TRUE(content.find("Trace side-effect") == std::string_view::npos);

    std::remove(log_file.c_str());
}

