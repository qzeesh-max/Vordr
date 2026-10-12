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
#include <fcntl.h>
#include <unistd.h>

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

TEST_CASE("MmapSink - Multiple Restarts Append Continuity") {
    const std::string test_path = "/tmp/vordr_test_restarts.log";
    std::remove(test_path.c_str());

    const std::string s1 = "[Session 1] Event A\n[Session 1] Event B\n";
    const std::string s2 = "[Session 2] Event C\n";
    const std::string s3 = "[Session 3] Event D\n[Session 3] Event E\n[Session 3] Event F\n";
    const std::string s4 = "[Session 4] Final Event G\n";

    // 1. Session 1
    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_EQ(sink.initial_offset(), 0u);
        ASSERT_TRUE(sink.write(s1));
        ASSERT_EQ(sink.bytes_written(), s1.size());
        ASSERT_EQ(sink.session_bytes_written(), s1.size());
        ASSERT_EQ(sink.session_content(), s1);
    }

    // 2. Session 2 (restart)
    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_EQ(sink.initial_offset(), s1.size());
        ASSERT_TRUE(sink.write(s2));
        ASSERT_EQ(sink.bytes_written(), s1.size() + s2.size());
        ASSERT_EQ(sink.session_bytes_written(), s2.size());
        ASSERT_EQ(sink.session_content(), s2);
        ASSERT_EQ(sink.written_content(), s1 + s2);
    }

    // 3. Session 3 (restart)
    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_EQ(sink.initial_offset(), s1.size() + s2.size());
        ASSERT_TRUE(sink.write(s3));
        ASSERT_EQ(sink.bytes_written(), s1.size() + s2.size() + s3.size());
        ASSERT_EQ(sink.session_bytes_written(), s3.size());
        ASSERT_EQ(sink.session_content(), s3);
        ASSERT_EQ(sink.written_content(), s1 + s2 + s3);
    }

    // 4. Session 4 (restart)
    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_EQ(sink.initial_offset(), s1.size() + s2.size() + s3.size());
        ASSERT_TRUE(sink.write(s4));
        ASSERT_EQ(sink.bytes_written(), s1.size() + s2.size() + s3.size() + s4.size());
        ASSERT_EQ(sink.session_bytes_written(), s4.size());
        ASSERT_EQ(sink.session_content(), s4);
        ASSERT_EQ(sink.written_content(), s1 + s2 + s3 + s4);
    }

    // 5. Verify entire disk file matches all sessions concatenated
    std::ifstream in(test_path, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string disk_content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_EQ(disk_content, s1 + s2 + s3 + s4);

    std::remove(test_path.c_str());
}

TEST_CASE("MmapSink - Unclean Crash Recovery and Trailing Null Trimming") {
    const std::string test_path = "/tmp/vordr_test_crash_recovery.log";
    std::remove(test_path.c_str());

    const std::string valid_lines = "Crash test line 1\nCrash test line 2\n";

    // Simulate an unclean crash: write valid data, then inflate file with ftruncate to 1 MB of nulls
    {
        int fd = ::open(test_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        ASSERT_TRUE(fd != -1);
        ASSERT_EQ(::write(fd, valid_lines.data(), valid_lines.size()), static_cast<ssize_t>(valid_lines.size()));
        // Abruptly inflate file to 1 MB with trailing nulls, simulating crash before close()
        ASSERT_EQ(::ftruncate(fd, 1024 * 1024), 0);
        ::close(fd);
    }

    // Now restart application with MmapSink
    const std::string append_line = "Recovered and appended line 3\n";
    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        // Should detect valid content size and ignore trailing 1 MB null padding!
        ASSERT_EQ(sink.initial_offset(), valid_lines.size());
        ASSERT_TRUE(sink.write(append_line));
        ASSERT_EQ(sink.bytes_written(), valid_lines.size() + append_line.size());
    }

    // Verify on disk: trailing null bytes must be trimmed, containing exactly valid + appended data
    std::ifstream in(test_path, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string disk_content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_EQ(disk_content, valid_lines + append_line);
    // Ensure no null bytes exist anywhere in the file
    ASSERT_EQ(disk_content.find('\0'), std::string::npos);

    std::remove(test_path.c_str());
}

TEST_CASE("MmapSink - Overwrite Mode When Append Is Disabled") {
    const std::string test_path = "/tmp/vordr_test_overwrite.log";
    std::remove(test_path.c_str());

    // 1. Initial write
    {
        vordr::MmapSink sink(test_path, 64 * 1024);
        ASSERT_TRUE(sink.write("Initial content to be overwritten\n"));
    }

    // 2. Open with append = false
    {
        vordr::MmapSink sink(test_path, 64 * 1024, /*append=*/false);
        ASSERT_EQ(sink.initial_offset(), 0u);
        ASSERT_TRUE(sink.write("Fresh new content\n"));
    }

    // 3. Verify disk file has only new content
    std::ifstream in(test_path, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    std::string disk_content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ASSERT_EQ(disk_content, "Fresh new content\n");

    std::remove(test_path.c_str());
}

