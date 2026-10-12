// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/timestamp.hpp"
#include <regex>
#include <thread>
#include <vector>
#include <sys/resource.h>

TEST_CASE("Timestamp - Format Length and Pattern") {
    char buf[64];
    size_t len = vordr::Timestamp::format_now(buf, sizeof(buf));
    ASSERT_EQ(len, vordr::Timestamp::FORMATTED_LENGTH);

    std::string str(buf, len);
    std::regex ts_regex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6}$)");
    ASSERT_TRUE(std::regex_match(str, ts_regex));
}

TEST_CASE("Timestamp - Microsecond Boundary Edge Cases") {
    char buf[64];
    // Create base timepoint at an exact second
    auto base_sec = std::chrono::sys_seconds{std::chrono::seconds{1700000000}};

    // 0 us -> .000000
    {
        auto tp = base_sec + std::chrono::microseconds{0};
        vordr::Timestamp::format_time_point(tp, buf, sizeof(buf));
        std::string_view sv(buf, 26);
        ASSERT_TRUE(sv.ends_with(".000000"));
    }

    // 1 us -> .000001
    {
        auto tp = base_sec + std::chrono::microseconds{1};
        vordr::Timestamp::format_time_point(tp, buf, sizeof(buf));
        std::string_view sv(buf, 26);
        ASSERT_TRUE(sv.ends_with(".000001"));
    }

    // 42 us -> .000042
    {
        auto tp = base_sec + std::chrono::microseconds{42};
        vordr::Timestamp::format_time_point(tp, buf, sizeof(buf));
        std::string_view sv(buf, 26);
        ASSERT_TRUE(sv.ends_with(".000042"));
    }

    // 999 us -> .000999
    {
        auto tp = base_sec + std::chrono::microseconds{999};
        vordr::Timestamp::format_time_point(tp, buf, sizeof(buf));
        std::string_view sv(buf, 26);
        ASSERT_TRUE(sv.ends_with(".000999"));
    }

    // 12345 us -> .012345
    {
        auto tp = base_sec + std::chrono::microseconds{12345};
        vordr::Timestamp::format_time_point(tp, buf, sizeof(buf));
        std::string_view sv(buf, 26);
        ASSERT_TRUE(sv.ends_with(".012345"));
    }

    // 999999 us -> .999999
    {
        auto tp = base_sec + std::chrono::microseconds{999999};
        vordr::Timestamp::format_time_point(tp, buf, sizeof(buf));
        std::string_view sv(buf, 26);
        ASSERT_TRUE(sv.ends_with(".999999"));
    }
}

TEST_CASE("Timestamp - Known Date Verification and Leap Year") {
    char buf[64];

    // Leap day: 2024-02-29 13:45:06.123456 UTC
    // 2024-02-29 13:45:06 = 1709214306 seconds
    {
        auto tp = std::chrono::sys_seconds{std::chrono::seconds{1709214306}} + std::chrono::microseconds{123456};
        vordr::Timestamp::format_time_point_utc(tp, buf, sizeof(buf));
        std::string str(buf, 26);
        ASSERT_EQ(str, "2024-02-29 13:45:06.123456");
    }

    // Century leap year: 2000-02-29 00:00:00.000000 UTC
    // 2000-02-29 00:00:00 = 951782400 seconds
    {
        auto tp = std::chrono::sys_seconds{std::chrono::seconds{951782400}};
        vordr::Timestamp::format_time_point_utc(tp, buf, sizeof(buf));
        std::string str(buf, 26);
        ASSERT_EQ(str, "2000-02-29 00:00:00.000000");
    }

    // Year-end rollover: 2026-12-31 23:59:59.999999 UTC
    // 2026-12-31 23:59:59 = 1798761599 seconds
    {
        auto tp = std::chrono::sys_seconds{std::chrono::seconds{1798761599}} + std::chrono::microseconds{999999};
        vordr::Timestamp::format_time_point_utc(tp, buf, sizeof(buf));
        std::string str(buf, 26);
        ASSERT_EQ(str, "2026-12-31 23:59:59.999999");
    }

    // Next second (New Year): 2027-01-01 00:00:00.000000 UTC
    // 2027-01-01 00:00:00 = 1798761600 seconds
    {
        auto tp = std::chrono::sys_seconds{std::chrono::seconds{1798761600}};
        vordr::Timestamp::format_time_point_utc(tp, buf, sizeof(buf));
        std::string str(buf, 26);
        ASSERT_EQ(str, "2027-01-01 00:00:00.000000");
    }
}

TEST_CASE("Timestamp - Cache Roll-over and Monotonicity") {
    char buf1[64], buf2[64], buf3[64];

    // Second T
    auto tp1 = std::chrono::sys_seconds{std::chrono::seconds{1700000000}} + std::chrono::microseconds{100000};
    vordr::Timestamp::format_time_point_utc(tp1, buf1, sizeof(buf1));

    // Same second T (should hit date cache)
    auto tp2 = std::chrono::sys_seconds{std::chrono::seconds{1700000000}} + std::chrono::microseconds{800000};
    vordr::Timestamp::format_time_point_utc(tp2, buf2, sizeof(buf2));

    ASSERT_EQ(std::string_view(buf1, 19), std::string_view(buf2, 19));
    ASSERT_TRUE(std::string_view(buf1, 26) != std::string_view(buf2, 26));

    // Next second T + 1
    auto tp3 = std::chrono::sys_seconds{std::chrono::seconds{1700000001}} + std::chrono::microseconds{100000};
    vordr::Timestamp::format_time_point_utc(tp3, buf3, sizeof(buf3));
    ASSERT_TRUE(std::string_view(buf1, 19) != std::string_view(buf3, 19));

    // Backward time step (NTP jump backwards to T - 10s)
    char buf4[64];
    auto tp4 = std::chrono::sys_seconds{std::chrono::seconds{1699999990}} + std::chrono::microseconds{100000};
    vordr::Timestamp::format_time_point_utc(tp4, buf4, sizeof(buf4));
    ASSERT_TRUE(std::string_view(buf3, 19) != std::string_view(buf4, 19));
}

TEST_CASE("Timestamp - Multi-Threaded Concurrency") {
    constexpr int NUM_THREADS = 8;
    constexpr size_t OPS_PER_THREAD = 10000;
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    std::atomic<bool> ok{true};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([t, &ok]() {
            char buf[64];
            int64_t base = 1700000000 + t * 1000;
            for (size_t i = 0; i < OPS_PER_THREAD; ++i) {
                auto tp = std::chrono::sys_seconds{std::chrono::seconds{base + static_cast<int64_t>(i % 10)}}
                        + std::chrono::microseconds{i % 1000000};
                size_t len = vordr::Timestamp::format_time_point_utc(tp, buf, sizeof(buf));
                if (len != vordr::Timestamp::FORMATTED_LENGTH) {
                    ok = false;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    ASSERT_TRUE(ok.load());
}

TEST_CASE("Timestamp - System Call Elimination Verification") {
    struct rusage ru_before{}, ru_after{};
    getrusage(RUSAGE_SELF, &ru_before);

    char buf[64];
    auto now = std::chrono::system_clock::now();
    constexpr size_t RUNS = 1'000'000;

    for (size_t i = 0; i < RUNS; ++i) {
        vordr::Timestamp::format_time_point_utc(now, buf, sizeof(buf));
    }

    getrusage(RUSAGE_SELF, &ru_after);

    long voluntary_cs = ru_after.ru_nvcsw - ru_before.ru_nvcsw;
    double sys_time = (ru_after.ru_stime.tv_sec - ru_before.ru_stime.tv_sec) +
                      (ru_after.ru_stime.tv_usec - ru_before.ru_stime.tv_usec) * 1e-6;

    // Zero system calls means zero blocking or voluntary context switches
    ASSERT_EQ(voluntary_cs, 0);
    // System CPU time should be negligible (< 0.05 seconds for 1M iterations)
    ASSERT_TRUE(sys_time < 0.05);
}
