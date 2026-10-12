// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/vordr.hpp"
#include <regex>
#include <fstream>
#include <thread>
#include <vector>
#include <cstdio>

namespace IntegrationTest {

class PaymentService {
public:
    void processPayment(int id) {
        VORDR_INFO << "Processing payment #" << id;
        VORDR_DEBUG << "Internal crypto key verified for payment #" << id;
    }
};

class AuditLogger {
public:
    void recordAudit(const std::string& action) {
        VORDR_WARN << "Security action recorded: " << action;
    }
};

} // namespace IntegrationTest

TEST_CASE("Integration - Full XML Configuration End-to-End") {
    const std::string log_file = "/tmp/vordr_integration.log";
    std::remove(log_file.c_str());

    constexpr std::string_view xml_config = R"(<?xml version="1.0" encoding="UTF-8"?>
<vordr>
    <sink path="/tmp/vordr_integration.log" max_size="1048576" />
    <logging level="INFO" emit_method="true">
        <class name="IntegrationTest::PaymentService" level="DEBUG" />
        <class name="IntegrationTest::AuditLogger" level="ERROR" />
    </logging>
</vordr>
)";

    auto& logger = vordr::Logger::instance();
    logger.reset();
    std::string err;
    bool ok = logger.init_xml_string(xml_config, &err);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(logger.config().emit_method());

    IntegrationTest::PaymentService payment;
    IntegrationTest::AuditLogger audit;

    // PaymentService has class level DEBUG:
    // INFO should be logged
    // DEBUG should be logged
    payment.processPayment(42);

    // AuditLogger has class level ERROR:
    // WARN should NOT be logged!
    audit.recordAudit("admin_login");

    logger.flush(true);
    std::string_view content = logger.sink()->written_content();

    // Check payment INFO and DEBUG are present with method names
    ASSERT_TRUE(content.find("Processing payment #42") != std::string_view::npos);
    ASSERT_TRUE(content.find("Internal crypto key verified for payment #42") != std::string_view::npos);
    ASSERT_TRUE(content.find("[IntegrationTest::PaymentService::processPayment]") != std::string_view::npos);

    // Check audit WARN was suppressed
    ASSERT_TRUE(content.find("Security action recorded") == std::string_view::npos);

    // Verify format of log lines with regex (thread ID included by default):
    // date-time upto microsecond - LogLevel - [tid:...] - Filename (without path) :Linenumber : [Method] Actual Log Entry
    std::regex line_regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6} - INFO - \[tid:\d+\] - test_integration\.cpp:\d+ : \[IntegrationTest::PaymentService::processPayment\] Processing payment #42\n)");
    ASSERT_TRUE(std::regex_search(std::string(content), line_regex));

    std::remove(log_file.c_str());
}

TEST_CASE("Integration - Disable Thread ID via XML Configuration") {
    const std::string log_file = "/tmp/vordr_no_tid.log";
    std::remove(log_file.c_str());

    constexpr std::string_view xml_config = R"(<?xml version="1.0" encoding="UTF-8"?>
<vordr>
    <sink path="/tmp/vordr_no_tid.log" max_size="1048576" />
    <logging level="INFO" emit_method="false" emit_thread_id="false" />
</vordr>
)";

    auto& logger = vordr::Logger::instance();
    logger.reset();
    std::string err;
    bool ok = logger.init_xml_string(xml_config, &err);
    ASSERT_TRUE(ok);
    ASSERT_FALSE(logger.config().emit_thread_id());

    VORDR_INFO << "Message without thread id";
    logger.flush(true);

    std::string_view content = logger.sink()->written_content();
    ASSERT_TRUE(content.find("Message without thread id") != std::string_view::npos);
    ASSERT_TRUE(content.find("[tid:") == std::string_view::npos);

    std::regex line_regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6} - INFO - test_integration\.cpp:\d+ : Message without thread id\n)");
    ASSERT_TRUE(std::regex_search(std::string(content), line_regex));

    std::remove(log_file.c_str());
}

TEST_CASE("Integration - Multi-Threaded Macro Logging") {
    const std::string log_file = "/tmp/vordr_threads.log";
    std::remove(log_file.c_str());

    auto& logger = vordr::Logger::instance();
    logger.reset();
    vordr::Config cfg;
    cfg.set_sink_path(log_file);
    cfg.set_central_level(vordr::LogLevel::INFO);
    cfg.set_emit_method(false);
    logger.init(std::move(cfg));

    constexpr int NUM_THREADS = 8;
    constexpr int MSGS_PER_THREAD = 100;

    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([t]() {
            for (int i = 0; i < MSGS_PER_THREAD; ++i) {
                VORDR_INFO << "Thread " << t << " iteration " << i;
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    logger.flush(true);
    std::string_view content = logger.sink()->written_content();
    ASSERT_TRUE(!content.empty());

    // Count occurrences of "Thread "
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("Thread ", pos)) != std::string_view::npos) {
        ++count;
        pos += 7;
    }

    ASSERT_EQ(count, static_cast<size_t>(NUM_THREADS * MSGS_PER_THREAD));

    std::remove(log_file.c_str());
}

TEST_CASE("Integration - Lazy Sink Initialization and Dynamic Reconfiguration") {
    const std::string log_file1 = "/tmp/vordr_lazy_sink1.log";
    const std::string log_file2 = "/tmp/vordr_lazy_sink2.log";
    std::remove(log_file1.c_str());
    std::remove(log_file2.c_str());

    auto& logger = vordr::Logger::instance();
    logger.reset();

    // 1. Initial configuration to log_file1
    vordr::Config cfg1;
    cfg1.set_sink_path(log_file1);
    cfg1.set_central_level(vordr::LogLevel::INFO);
    logger.init(std::move(cfg1));

    VORDR_INFO << "Message written to sink 1";
    logger.flush(true);
    ASSERT_TRUE(logger.sink()->bytes_written() > 0);

    // 2. Dynamic re-initialization to log_file2
    vordr::Config cfg2;
    cfg2.set_sink_path(log_file2);
    cfg2.set_central_level(vordr::LogLevel::INFO);
    logger.init(std::move(cfg2));

    VORDR_INFO << "Message written to sink 2";
    logger.flush(true);
    ASSERT_TRUE(logger.sink()->bytes_written() > 0);

    logger.close();

    // 3. Verify both files exist and are non-empty directly on the filesystem
    std::ifstream f1(log_file1, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f1.is_open());
    auto sz1 = f1.tellg();
    ASSERT_TRUE(sz1 > 0);

    std::ifstream f2(log_file2, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f2.is_open());
    auto sz2 = f2.tellg();
    ASSERT_TRUE(sz2 > 0);

    std::remove(log_file1.c_str());
    std::remove(log_file2.c_str());
}

TEST_CASE("Integration - Multi-Restart Macro Logging Across Application Lifecycles") {
    const std::string log_file = "/tmp/vordr_lifecycle_restart.log";
    std::remove(log_file.c_str());

    auto& logger = vordr::Logger::instance();

    // Lifecycle 1
    {
        logger.reset();
        vordr::Config cfg;
        cfg.set_sink_path(log_file);
        cfg.set_central_level(vordr::LogLevel::INFO);
        cfg.set_emit_thread_id(false);
        logger.init(std::move(cfg));

        VORDR_INFO << "Lifecycle 1: system started";
        VORDR_INFO << "Lifecycle 1: workload executed";
        logger.close();
    }

    // Lifecycle 2 (simulating restart)
    {
        logger.reset();
        vordr::Config cfg;
        cfg.set_sink_path(log_file);
        cfg.set_central_level(vordr::LogLevel::INFO);
        cfg.set_emit_thread_id(false);
        logger.init(std::move(cfg));

        VORDR_INFO << "Lifecycle 2: system restarted";
        logger.close();
    }

    // Lifecycle 3 (simulating restart)
    {
        logger.reset();
        vordr::Config cfg;
        cfg.set_sink_path(log_file);
        cfg.set_central_level(vordr::LogLevel::INFO);
        cfg.set_emit_thread_id(false);
        logger.init(std::move(cfg));

        VORDR_INFO << "Lifecycle 3: post-reboot healthcheck OK";
        logger.close();
    }

    // Read full file from disk and assert all 4 entries are present in sequence
    std::ifstream in(log_file, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    ASSERT_TRUE(content.find("Lifecycle 1: system started") != std::string::npos);
    ASSERT_TRUE(content.find("Lifecycle 1: workload executed") != std::string::npos);
    ASSERT_TRUE(content.find("Lifecycle 2: system restarted") != std::string::npos);
    ASSERT_TRUE(content.find("Lifecycle 3: post-reboot healthcheck OK") != std::string::npos);

    // Verify ordering
    size_t pos1 = content.find("Lifecycle 1: system started");
    size_t pos2 = content.find("Lifecycle 1: workload executed");
    size_t pos3 = content.find("Lifecycle 2: system restarted");
    size_t pos4 = content.find("Lifecycle 3: post-reboot healthcheck OK");
    ASSERT_TRUE(pos1 < pos2);
    ASSERT_TRUE(pos2 < pos3);
    ASSERT_TRUE(pos3 < pos4);

    std::remove(log_file.c_str());
}


