// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <cstdint>
#include <cstddef>
#include <charconv>
#include <string_view>

#if defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>
#else
#include <thread>
#endif

namespace vordr {

class ThreadId {
public:
    static constexpr size_t BUFFER_SIZE = 32;

    static uint64_t get() noexcept {
        return cache_.id_;
    }

    static std::string_view get_formatted() noexcept {
        return std::string_view(cache_.formatted_, cache_.length_);
    }

private:
    struct Cache {
        uint64_t id_{0};
        char formatted_[BUFFER_SIZE]{};
        size_t length_{0};

        Cache() noexcept {
            id_ = query_os_tid();

            // Format as "[tid:<id>]"
            formatted_[0] = '[';
            formatted_[1] = 't';
            formatted_[2] = 'i';
            formatted_[3] = 'd';
            formatted_[4] = ':';
            auto [ptr, ec] = std::to_chars(formatted_ + 5, formatted_ + sizeof(formatted_) - 1, id_);
            if (ec == std::errc{}) {
                *ptr++ = ']';
                length_ = static_cast<size_t>(ptr - formatted_);
            } else {
                formatted_[5] = '0';
                formatted_[6] = ']';
                length_ = 7;
            }
        }

        static uint64_t query_os_tid() noexcept {
#if defined(__APPLE__)
            uint64_t tid = 0;
            if (pthread_threadid_np(nullptr, &tid) == 0) {
                return tid;
            }
            return reinterpret_cast<uint64_t>(pthread_self());
#elif defined(__linux__)
            return static_cast<uint64_t>(::syscall(SYS_gettid));
#else
            return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
        }
    };

    static inline thread_local Cache cache_{};
};

} // namespace vordr
