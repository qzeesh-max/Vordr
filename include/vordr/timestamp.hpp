// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <chrono>
#include <ctime>
#include <cstdint>
#include <cstring>

namespace vordr {

class Timestamp {
public:
    static constexpr size_t FORMATTED_LENGTH = 26; // "YYYY-MM-DD HH:MM:SS.ffffff"

    static size_t format_now(char* out, size_t max_len) noexcept {
        return format_time_point(std::chrono::system_clock::now(), out, max_len);
    }

    template <typename Clock, typename Duration>
    static size_t format_time_point(const std::chrono::time_point<Clock, Duration>& tp,
                                    char* out, size_t max_len) noexcept
    {
        if (max_len < FORMATTED_LENGTH) {
            return 0;
        }

        auto duration = tp.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration - seconds).count();
        if (micros < 0) micros = 0;

        std::time_t tt = seconds.count();
        std::tm tm_buf{};
#if defined(_WIN32)
        gmtime_s(&tm_buf, &tt);
#else
        gmtime_r(&tt, &tm_buf);
#endif

        int year = tm_buf.tm_year + 1900;
        int month = tm_buf.tm_mon + 1;
        int day = tm_buf.tm_mday;
        int hour = tm_buf.tm_hour;
        int min = tm_buf.tm_min;
        int sec = tm_buf.tm_sec;

        // YYYY-MM-DD HH:MM:SS.ffffff
        // Fast digit writing
        out[0] = static_cast<char>('0' + (year / 1000) % 10);
        out[1] = static_cast<char>('0' + (year / 100) % 10);
        out[2] = static_cast<char>('0' + (year / 10) % 10);
        out[3] = static_cast<char>('0' + (year % 10));
        out[4] = '-';

        out[5] = static_cast<char>('0' + (month / 10));
        out[6] = static_cast<char>('0' + (month % 10));
        out[7] = '-';

        out[8] = static_cast<char>('0' + (day / 10));
        out[9] = static_cast<char>('0' + (day % 10));
        out[10] = ' ';

        out[11] = static_cast<char>('0' + (hour / 10));
        out[12] = static_cast<char>('0' + (hour % 10));
        out[13] = ':';

        out[14] = static_cast<char>('0' + (min / 10));
        out[15] = static_cast<char>('0' + (min % 10));
        out[16] = ':';

        out[17] = static_cast<char>('0' + (sec / 10));
        out[18] = static_cast<char>('0' + (sec % 10));
        out[19] = '.';

        // 6 digits of microsecond
        out[20] = static_cast<char>('0' + (micros / 100000) % 10);
        out[21] = static_cast<char>('0' + (micros / 10000) % 10);
        out[22] = static_cast<char>('0' + (micros / 1000) % 10);
        out[23] = static_cast<char>('0' + (micros / 100) % 10);
        out[24] = static_cast<char>('0' + (micros / 10) % 10);
        out[25] = static_cast<char>('0' + (micros % 10));

        return FORMATTED_LENGTH;
    }
};

} // namespace vordr
