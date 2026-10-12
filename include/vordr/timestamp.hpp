// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <chrono>
#include <ctime>
#include <cstdint>
#include <cstring>
#include <atomic>

namespace vordr {

enum class TimestampMode {
    UTC,
    LOCAL
};

namespace detail {

alignas(64) inline constexpr char TWO_DIGITS[100][2] = {
    {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'}, {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
    {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'}, {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
    {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'}, {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
    {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'}, {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
    {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'}, {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
    {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'}, {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
    {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'}, {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
    {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'}, {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
    {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'}, {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
    {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'}, {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
};

struct DateCache {
    int64_t cached_second{-1};
    char buffer[21]{}; // "YYYY-MM-DD HH:MM:SS."
};

inline thread_local DateCache t_date_cache{};

// Pure user-space civil calendar calculation (C++20 chrono)
// Zero system calls, zero libc locks, completely re-entrant
inline void format_date_prefix(int64_t sec, char* out) noexcept {
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

    // Year: YYYY-
    out[0] = static_cast<char>('0' + (y / 1000) % 10);
    out[1] = static_cast<char>('0' + (y / 100) % 10);
    out[2] = static_cast<char>('0' + (y / 10) % 10);
    out[3] = static_cast<char>('0' + (y % 10));
    out[4] = '-';

    // Month: MM-
    std::memcpy(out + 5, TWO_DIGITS[m], 2);
    out[7] = '-';

    // Day: DD<space>
    std::memcpy(out + 8, TWO_DIGITS[d], 2);
    out[10] = ' ';

    // Hour: HH:
    std::memcpy(out + 11, TWO_DIGITS[h], 2);
    out[13] = ':';

    // Minute: MM:
    std::memcpy(out + 14, TWO_DIGITS[min], 2);
    out[16] = ':';

    // Second: SS.
    std::memcpy(out + 17, TWO_DIGITS[s], 2);
    out[19] = '.';
}

// Cached timezone offset: recomputed at most once per day or upon initialization
inline std::atomic<int64_t> g_cached_tz_offset{0};
inline std::atomic<int64_t> g_tz_cached_day{-1};

inline int64_t resolve_local_offset(int64_t utc_sec) noexcept {
    int64_t current_day = utc_sec / 86400;
    int64_t cached_day = g_tz_cached_day.load(std::memory_order_relaxed);

    if (__builtin_expect(current_day != cached_day, 0)) {
        time_t t = static_cast<time_t>(utc_sec);
        struct tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &t);
        // Approximation or _get_timezone
        long tz_seconds = 0;
        _get_timezone(&tz_seconds);
        int64_t offset = -tz_seconds + (tm_buf.tm_isdst > 0 ? 3600 : 0);
#else
        localtime_r(&t, &tm_buf);
#if defined(__APPLE__) || defined(__linux__) || defined(__GLIBC__)
        int64_t offset = tm_buf.tm_gmtoff;
#else
        struct tm gmt_buf{};
        gmtime_r(&t, &gmt_buf);
        int64_t offset = static_cast<int64_t>(difftime(mktime(&tm_buf), mktime(&gmt_buf)));
#endif
#endif
        g_cached_tz_offset.store(offset, std::memory_order_relaxed);
        g_tz_cached_day.store(current_day, std::memory_order_release);
        return offset;
    }
    return g_cached_tz_offset.load(std::memory_order_relaxed);
}

} // namespace detail

class Timestamp {
public:
    static constexpr size_t FORMATTED_LENGTH = 26; // "YYYY-MM-DD HH:MM:SS.ffffff"

    static TimestampMode mode() noexcept {
        return mode_.load(std::memory_order_relaxed);
    }

    static void set_mode(TimestampMode m) noexcept {
        mode_.store(m, std::memory_order_relaxed);
    }

    static size_t format_now(char* out, size_t max_len) noexcept {
        return format_time_point(std::chrono::system_clock::now(), out, max_len);
    }

    template <typename Clock, typename Duration>
    static size_t format_time_point(const std::chrono::time_point<Clock, Duration>& tp,
                                    char* out, size_t max_len) noexcept
    {
        if (mode_.load(std::memory_order_relaxed) == TimestampMode::LOCAL) {
            return format_time_point_local(tp, out, max_len);
        }
        return format_time_point_utc(tp, out, max_len);
    }

    template <typename Clock, typename Duration>
    static size_t format_time_point_utc(const std::chrono::time_point<Clock, Duration>& tp,
                                        char* out, size_t max_len) noexcept
    {
        return format_impl(tp, 0, out, max_len);
    }

    template <typename Clock, typename Duration>
    static size_t format_time_point_local(const std::chrono::time_point<Clock, Duration>& tp,
                                          char* out, size_t max_len) noexcept
    {
        auto duration = tp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        int64_t offset = detail::resolve_local_offset(seconds.count());
        return format_impl(tp, offset, out, max_len);
    }

private:
    template <typename Clock, typename Duration>
    static size_t format_impl(const std::chrono::time_point<Clock, Duration>& tp,
                              int64_t offset_seconds,
                              char* out, size_t max_len) noexcept
    {
        if (__builtin_expect(max_len < FORMATTED_LENGTH, 0)) {
            return 0;
        }

        auto duration = tp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds).count();
        if (micros < 0) micros = 0;

        int64_t effective_sec = seconds.count() + offset_seconds;

        // Check per-thread cached date prefix
        auto& cache = detail::t_date_cache;
        if (__builtin_expect(effective_sec != cache.cached_second, 0)) {
            cache.cached_second = effective_sec;
            detail::format_date_prefix(effective_sec, cache.buffer);
        }

        // Fast 20-byte copy: "YYYY-MM-DD HH:MM:SS."
        std::memcpy(out, cache.buffer, 20);

        // Fast 6-digit microsecond formatting using two-digit lookup table
        uint32_t u = static_cast<uint32_t>(micros);
        uint32_t d1 = u / 10000;
        uint32_t rem = u - d1 * 10000;
        uint32_t d2 = rem / 100;
        uint32_t d3 = rem - d2 * 100;

        std::memcpy(out + 20, detail::TWO_DIGITS[d1], 2);
        std::memcpy(out + 22, detail::TWO_DIGITS[d2], 2);
        std::memcpy(out + 24, detail::TWO_DIGITS[d3], 2);

        return FORMATTED_LENGTH;
    }

    static inline std::atomic<TimestampMode> mode_{TimestampMode::UTC};
};

} // namespace vordr
