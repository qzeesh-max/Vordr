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

MmapSink::MmapSink(std::string file_path, size_t initial_capacity)
    : file_path_(std::move(file_path))
{
    size_t cap = std::max(initial_capacity, MIN_CAPACITY);
    fd_ = ::open(file_path_.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ == -1) {
        return;
    }

    struct stat st{};
    if (::fstat(fd_, &st) == 0 && st.st_size > static_cast<off_t>(cap)) {
        cap = static_cast<size_t>(st.st_size);
    } else {
        if (::ftruncate(fd_, static_cast<off_t>(cap)) != 0) {
            ::close(fd_);
            fd_ = -1;
            return;
        }
    }

    void* ptr = ::mmap(nullptr, cap, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd_);
        fd_ = -1;
        return;
    }

    mmap_ptr_ = static_cast<char*>(ptr);
    capacity_.store(cap, std::memory_order_release);
    write_cursor_.store(0, std::memory_order_release);
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
            ::ftruncate(fd_, static_cast<off_t>(written));
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

} // namespace vordr
