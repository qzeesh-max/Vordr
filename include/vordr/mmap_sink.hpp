// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <string>
#include <string_view>
#include <atomic>
#include <mutex>
#include <cstddef>
#include <cstdint>

namespace vordr {

class MmapSink {
public:
    static constexpr size_t DEFAULT_CAPACITY = 64 * 1024 * 1024; // 64 MB
    static constexpr size_t MIN_CAPACITY     = 64 * 1024;       // 64 KB

    explicit MmapSink(std::string file_path, size_t initial_capacity = DEFAULT_CAPACITY, bool append = true);
    ~MmapSink();

    // Non-copyable, non-movable for safety of atomic state and mmap pointers
    MmapSink(const MmapSink&) = delete;
    MmapSink& operator=(const MmapSink&) = delete;
    MmapSink(MmapSink&&) = delete;
    MmapSink& operator=(MmapSink&&) = delete;

    // Lock-free write method
    bool write(const char* data, size_t len) noexcept;
    bool write(std::string_view sv) noexcept {
        return write(sv.data(), sv.size());
    }

    // Direct slice reservation for zero-copy in-place writing
    char* reserve_slice(size_t len) noexcept;

    void flush(bool sync = false) noexcept;
    void close() noexcept;

    size_t bytes_written() const noexcept {
        return write_cursor_.load(std::memory_order_acquire);
    }

    size_t initial_offset() const noexcept {
        return initial_offset_;
    }

    size_t session_bytes_written() const noexcept {
        size_t current = write_cursor_.load(std::memory_order_acquire);
        return current >= initial_offset_ ? current - initial_offset_ : 0;
    }

    size_t capacity() const noexcept {
        return capacity_.load(std::memory_order_acquire);
    }

    const std::string& path() const noexcept {
        return file_path_;
    }

    // Inspect mapped contents (useful for testing & reading logged entries)
    std::string_view written_content() const noexcept;

    // Inspect content written during the current session only
    std::string_view session_content() const noexcept;

    const std::string& rotated_file_path() const noexcept {
        return rotated_file_path_;
    }

    static std::string generate_rotated_path(const std::string& base_path);
    static size_t scan_valid_content_size(int fd, size_t file_size) noexcept;

    bool is_open() const noexcept {
        return fd_ != -1 && mmap_ptr_ != nullptr;
    }

private:
    bool remap_grow(size_t required_capacity) noexcept;

    std::string file_path_;
    std::string rotated_file_path_;
    int fd_{-1};
    char* mmap_ptr_{nullptr};
    std::atomic<size_t> capacity_{0};
    std::atomic<size_t> write_cursor_{0};
    size_t initial_offset_{0};
    std::mutex expand_mutex_;
};

} // namespace vordr
