// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/mmap_sink.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

namespace vordr {

namespace {

inline bool is_block_all_zeros(const char* ptr, size_t len) noexcept {
    const size_t words = len / 8;
    for (size_t i = 0; i < words; ++i) {
        uint64_t val = 0;
        std::memcpy(&val, ptr + i * 8, 8);
        if (val != 0) {
            return false;
        }
    }
    const size_t rem = len % 8;
    const size_t rem_start = words * 8;
    for (size_t i = 0; i < rem; ++i) {
        if (ptr[rem_start + i] != '\0') {
            return false;
        }
    }
    return true;
}

} // namespace

std::string MmapSink::generate_rotated_path(const std::string& base_path) {
    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto tp = std::chrono::sys_seconds{std::chrono::seconds{sec}};
    auto dp = std::chrono::floor<std::chrono::days>(tp);
    std::chrono::year_month_day ymd{dp};
    std::chrono::hh_mm_ss time{tp - dp};

    int y = static_cast<int>(ymd.year());
    unsigned m = static_cast<unsigned>(ymd.month());
    unsigned d = static_cast<unsigned>(ymd.day());
    long h = time.hours().count();
    long min = time.minutes().count();
    long s = time.seconds().count();

    char buf[64];
    std::snprintf(buf, sizeof(buf), ".%04d%02u%02u_%02ld%02ld%02ld",
                  y, m, d, h, min, s);

    std::string candidate = base_path + buf;
    struct stat st{};
    if (::stat(candidate.c_str(), &st) != 0) {
        return candidate;
    }

    for (int suffix = 1; suffix < 10000; ++suffix) {
        std::string disambiguated = candidate + "." + std::to_string(suffix);
        if (::stat(disambiguated.c_str(), &st) != 0) {
            return disambiguated;
        }
    }
    return candidate;
}

size_t MmapSink::scan_valid_content_size(int fd, size_t file_size) noexcept {
    if (file_size == 0 || fd == -1) {
        return 0;
    }

    void* initial_map = ::mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (initial_map == MAP_FAILED) {
        return 0;
    }

    const char* p = static_cast<const char*>(initial_map);

    // Fast-path: if the last byte is non-null (clean shutdown or full capacity), return full size immediately!
    if (p[file_size - 1] != '\0') {
        ::munmap(initial_map, file_size);
        return file_size;
    }

    constexpr size_t BLOCK_SIZE = 512;
    size_t first_block_len = std::min(file_size, BLOCK_SIZE);
    if (is_block_all_zeros(p, first_block_len)) {
        ::munmap(initial_map, file_size);
        return 0;
    }

    // Level 1: Logarithmic search across 512-byte blocks
    size_t num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t low = 0;
    size_t high = num_blocks - 1;

    while (low < high) {
        size_t mid = low + (high - low + 1) / 2;
        size_t offset = mid * BLOCK_SIZE;
        size_t len = std::min(BLOCK_SIZE, file_size - offset);
        if (is_block_all_zeros(p + offset, len)) {
            high = mid - 1;
        } else {
            low = mid;
        }
    }

    // Level 2: Logarithmic search within the transition block
    size_t b_start = low * BLOCK_SIZE;
    size_t b_len = std::min(BLOCK_SIZE, file_size - b_start);

    size_t sub_low = b_start;
    size_t sub_high = b_start + b_len - 1;

    while (sub_low < sub_high) {
        size_t mid = sub_low + (sub_high - sub_low + 1) / 2;
        size_t check_len = sub_high - mid + 1;
        if (is_block_all_zeros(p + mid, check_len)) {
            sub_high = mid - 1;
        } else {
            sub_low = mid;
        }
    }

    size_t valid_size = (p[sub_low] != '\0') ? (sub_low + 1) : 0;
    ::munmap(initial_map, file_size);
    return valid_size;
}

MmapSink::MmapSink(std::string file_path, size_t initial_capacity, bool append)
    : file_path_(std::move(file_path))
{
    if (!append) {
        struct stat st{};
        if (::stat(file_path_.c_str(), &st) == 0 && st.st_size > 0) {
            std::string rot = generate_rotated_path(file_path_);
            if (::rename(file_path_.c_str(), rot.c_str()) == 0) {
                rotated_file_path_ = std::move(rot);
            }
        }
    }

    fd_ = ::open(file_path_.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ == -1) {
        return;
    }

    size_t resume_pos = 0;
    if (append) {
        struct stat st{};
        if (::fstat(fd_, &st) == 0 && st.st_size > 0) {
            resume_pos = scan_valid_content_size(fd_, static_cast<size_t>(st.st_size));
        }
    }

    initial_offset_ = resume_pos;
    size_t cap = std::max(resume_pos + initial_capacity, MIN_CAPACITY);

    if (::ftruncate(fd_, static_cast<off_t>(cap)) != 0) {
        ::close(fd_);
        fd_ = -1;
        return;
    }

    void* ptr = ::mmap(nullptr, cap, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd_);
        fd_ = -1;
        return;
    }

    mmap_ptr_ = static_cast<char*>(ptr);
    capacity_.store(cap, std::memory_order_release);
    write_cursor_.store(resume_pos, std::memory_order_release);
}

MmapSink::~MmapSink() {
    close();
}

bool MmapSink::write(const char* data, size_t len) noexcept {
    if (!is_open() || len == 0 || !data) {
        return false;
    }

    // Fast-path lock-free write reservation
    size_t offset = write_cursor_.fetch_add(len, std::memory_order_acq_rel);
    size_t cap = capacity_.load(std::memory_order_acquire);

    if (offset + len <= cap) {
        std::memcpy(mmap_ptr_ + offset, data, len);
        return true;
    }

    // Slow-path: buffer capacity exceeded, expand under mutex
    std::lock_guard lock(expand_mutex_);
    if (!remap_grow(offset + len)) {
        return false;
    }

    std::memcpy(mmap_ptr_ + offset, data, len);
    return true;
}

char* MmapSink::reserve_slice(size_t len) noexcept {
    if (!is_open() || len == 0) {
        return nullptr;
    }

    size_t offset = write_cursor_.fetch_add(len, std::memory_order_acq_rel);
    size_t cap = capacity_.load(std::memory_order_acquire);

    if (offset + len <= cap) {
        return mmap_ptr_ + offset;
    }

    std::lock_guard lock(expand_mutex_);
    if (!remap_grow(offset + len)) {
        return nullptr;
    }

    return mmap_ptr_ + offset;
}

bool MmapSink::remap_grow(size_t required_capacity) noexcept {
    size_t current_cap = capacity_.load(std::memory_order_relaxed);
    if (required_capacity <= current_cap) {
        return true;
    }

    size_t new_cap = std::max(current_cap * 2, required_capacity + 1024 * 1024);

    if (::ftruncate(fd_, static_cast<off_t>(new_cap)) != 0) {
        return false;
    }

    if (mmap_ptr_ != nullptr) {
        ::msync(mmap_ptr_, current_cap, MS_SYNC);
        ::munmap(mmap_ptr_, current_cap);
    }

    void* new_ptr = ::mmap(nullptr, new_cap, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (new_ptr == MAP_FAILED) {
        mmap_ptr_ = nullptr;
        capacity_.store(0, std::memory_order_release);
        return false;
    }

    mmap_ptr_ = static_cast<char*>(new_ptr);
    capacity_.store(new_cap, std::memory_order_release);
    return true;
}

void MmapSink::flush(bool sync) noexcept {
    if (is_open()) {
        size_t written = write_cursor_.load(std::memory_order_acquire);
        size_t sync_len = std::min(written, capacity_.load(std::memory_order_acquire));
        if (sync_len > 0) {
            ::msync(mmap_ptr_, sync_len, sync ? MS_SYNC : MS_ASYNC);
        }
    }
}

void MmapSink::close() noexcept {
    if (is_open()) {
        size_t written = write_cursor_.load(std::memory_order_acquire);
        size_t cap = capacity_.load(std::memory_order_acquire);

        flush(true);
        ::munmap(mmap_ptr_, cap);
        mmap_ptr_ = nullptr;

        // Truncate file down to actual written size
        if (fd_ != -1) {
            if (::ftruncate(fd_, static_cast<off_t>(written)) < 0) {
                // Ignore truncation failure on close
            }
            ::close(fd_);
            fd_ = -1;
        }
    }
}

std::string_view MmapSink::written_content() const noexcept {
    if (!is_open()) return {};
    size_t written = write_cursor_.load(std::memory_order_acquire);
    size_t cap = capacity_.load(std::memory_order_acquire);
    return std::string_view(mmap_ptr_, std::min(written, cap));
}

std::string_view MmapSink::session_content() const noexcept {
    if (!is_open()) return {};
    size_t written = write_cursor_.load(std::memory_order_acquire);
    size_t cap = capacity_.load(std::memory_order_acquire);
    size_t total = std::min(written, cap);
    if (total <= initial_offset_) return {};
    return std::string_view(mmap_ptr_ + initial_offset_, total - initial_offset_);
}

} // namespace vordr
