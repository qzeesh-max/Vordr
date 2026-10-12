// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/mmap_sink.hpp"
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdio>

TEST_CASE("MmapSink - Basic Write and Readback") {
    const std::string test_path = "/tmp/vordr_test_basic.log";
    std::remove(test_path.c_str());

    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_TRUE(sink.is_open());

        bool ok1 = sink.write("Hello Vordr!\n");
        bool ok2 = sink.write("Second line of log.\n");
        ASSERT_TRUE(ok1);
        ASSERT_TRUE(ok2);

        std::string_view content = sink.written_content();
        ASSERT_EQ(content, "Hello Vordr!\nSecond line of log.\n");
    }

    // Verify persisted on disk after sink is closed
    std::ifstream in(test_path);
    ASSERT_TRUE(in.is_open());
    std::string disk_content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_EQ(disk_content, "Hello Vordr!\nSecond line of log.\n");

    std::remove(test_path.c_str());
}

TEST_CASE("MmapSink - Auto Expansion Beyond Initial Capacity") {
    const std::string test_path = "/tmp/vordr_test_expand.log";
    std::remove(test_path.c_str());

    {
        // Start with minimum capacity (64 KB)
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_TRUE(sink.is_open());

        // Write 128 KB of data (exceeding initial capacity)
        std::string chunk(1024, 'X');
        chunk += '\n';

        for (int i = 0; i < 120; ++i) {
            bool ok = sink.write(chunk);
            ASSERT_TRUE(ok);
        }

        ASSERT_TRUE(sink.bytes_written() >= 120 * 1025);
        ASSERT_TRUE(sink.capacity() > 64 * 1024);
    }

    std::remove(test_path.c_str());
}

TEST_CASE("MmapSink - Concurrent Multi-Threaded Stress Test") {
    const std::string test_path = "/tmp/vordr_test_concurrent.log";
    std::remove(test_path.c_str());

    constexpr int NUM_THREADS = 16;
    constexpr int MSGS_PER_THREAD = 500;

    {
        vordr::MmapSink sink(test_path, 1024 * 1024);
        ASSERT_TRUE(sink.is_open());

        std::vector<std::thread> workers;
        workers.reserve(NUM_THREADS);

        for (int t = 0; t < NUM_THREADS; ++t) {
            workers.emplace_back([&sink, t]() {
                for (int m = 0; m < MSGS_PER_THREAD; ++m) {
                    char msg[128];
                    int len = std::snprintf(msg, sizeof(msg), "[THREAD_%02d_MSG_%04d]\n", t, m);
                    sink.write(msg, static_cast<size_t>(len));
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }

        sink.flush(true);
        std::string_view content = sink.written_content();
        ASSERT_EQ(sink.bytes_written(), NUM_THREADS * MSGS_PER_THREAD * 21u); // each msg is "[THREAD_00_MSG_0000]\n" = 21 chars

        // Verify that every thread and message is present
        for (int t = 0; t < NUM_THREADS; ++t) {
            char tag[64];
            std::snprintf(tag, sizeof(tag), "[THREAD_%02d_MSG_0000]\n", t);
            ASSERT_TRUE(content.find(tag) != std::string_view::npos);

            std::snprintf(tag, sizeof(tag), "[THREAD_%02d_MSG_%04d]\n", t, MSGS_PER_THREAD - 1);
            ASSERT_TRUE(content.find(tag) != std::string_view::npos);
        }
    }

    std::remove(test_path.c_str());
}
