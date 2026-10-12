// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/mmap_sink.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace vordr {

size_t MmapSink::scan_valid_content_size(int fd, size_t file_size) noexcept {
    if (file_size == 0 || fd == -1) {
        return 0;
    }

    void* initial_map = ::mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (initial_map == MAP_FAILED) {
        return 0;
    }

    const char* p = static_cast<const char*>(initial_map);
    ssize_t last = static_cast<ssize_t>(file_size) - 1;

    // Fast-path: if the last byte is non-null (clean shutdown), return full size immediately!
    if (p[last] != '\0') {
        ::munmap(initial_map, file_size);
        return file_size;
    }

    // Backward scan for unclean shutdown recovery (skipping 64-bit zero blocks)
    while (last >= 0 && (last % 8 != 7) && p[last] == '\0') {
        --last;
    }
    while (last >= 7) {
        uint64_t val = 0;
        std::memcpy(&val, p + last - 7, 8);
        if (val != 0) {
            while (last >= 0 && p[last] == '\0') {
                --last;
            }
            break;
        }
        last -= 8;
    }
    while (last >= 0 && p[last] == '\0') {
        --last;
    }

    size_t valid_size = (last >= 0) ? static_cast<size_t>(last + 1) : 0;
    ::munmap(initial_map, file_size);
    return valid_size;
}

MmapSink::MmapSink(std::string file_path, size_t initial_capacity, bool append)
    : file_path_(std::move(file_path))
{
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
