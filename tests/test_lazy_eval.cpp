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
