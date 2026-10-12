// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <streambuf>
#include <ostream>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <cstring>
#include "vordr/types.hpp"
#include "vordr/timestamp.hpp"
#include "vordr/thread_id.hpp"

namespace vordr {

class StackStreamBuf : public std::streambuf {
public:
    StackStreamBuf(char* buffer, size_t capacity) noexcept
        : buffer_(buffer), capacity_(capacity)
    {
        setp(buffer_, buffer_ + capacity_);
    }

    size_t size() const noexcept {
        return static_cast<size_t>(pptr() - pbase());
    }

    size_t capacity() const noexcept {
        return capacity_;
    }

    void reset() noexcept {
        setp(buffer_, buffer_ + capacity_);
    }

    const char* data() const noexcept {
        return buffer_;
    }

    std::string_view view() const noexcept {
        return std::string_view(buffer_, size());
    }

protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::streamsize avail = epptr() - pptr();
        std::streamsize write_len = (n <= avail) ? n : avail;
        if (write_len > 0) {
            std::memcpy(pptr(), s, static_cast<size_t>(write_len));
            pbump(static_cast<int>(write_len));
        }
        return write_len;
    }

    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof() && pptr() < epptr()) {
            *pptr() = traits_type::to_char_type(ch);
            pbump(1);
            return ch;
        }
        return traits_type::eof();
    }

private:
    char* buffer_;
    size_t capacity_;
};

template <size_t BufferSize = 4096>
class LogStream : public std::ostream {
public:
    LogStream() noexcept
        : std::ostream(&streambuf_), streambuf_(buffer_, BufferSize) {}

    explicit LogStream(char* external_buffer, size_t capacity) noexcept
        : std::ostream(&streambuf_), streambuf_(external_buffer, capacity) {}

    // Fast-path zero-allocation overloads
    LogStream& operator<<(std::string_view sv) {
        write_fast(sv.data(), sv.size());
        return *this;
    }

    LogStream& operator<<(const char* str) {
        if (str) {
            write_fast(str, std::strlen(str));
        }
        return *this;
    }

    LogStream& operator<<(char ch) {
        write_fast(&ch, 1);
        return *this;
    }

    LogStream& operator<<(bool b) {
        if (b) {
            write_fast("true", 4);
        } else {
            write_fast("false", 5);
        }
        return *this;
    }

    template <typename IntT>
        requires (std::is_integral_v<IntT> && !std::is_same_v<IntT, bool> && !std::is_same_v<IntT, char>)
    LogStream& operator<<(IntT val) {
        char temp[32];
        auto [ptr, ec] = std::to_chars(temp, temp + sizeof(temp), val);
        if (ec == std::errc{}) {
            write_fast(temp, static_cast<size_t>(ptr - temp));
        }
        return *this;
    }

    template <typename FloatT>
        requires std::is_floating_point_v<FloatT>
    LogStream& operator<<(FloatT val) {
        char temp[64];
        auto [ptr, ec] = std::to_chars(temp, temp + sizeof(temp), val);
        if (ec == std::errc{}) {
            write_fast(temp, static_cast<size_t>(ptr - temp));
        } else {
            static_cast<std::ostream&>(*this) << val;
        }
        return *this;
    }

    LogStream& operator<<(const void* ptr) {
        write_fast("0x", 2);
        char temp[32];
        auto [p, ec] = std::to_chars(temp, temp + sizeof(temp), reinterpret_cast<uintptr_t>(ptr), 16);
        if (ec == std::errc{}) {
            write_fast(temp, static_cast<size_t>(p - temp));
        }
        return *this;
    }

    LogStream& operator<<(LogLevel level) {
        std::string_view s = to_string(level);
        write_fast(s.data(), s.size());
        return *this;
    }

    // Fallback for custom types that have std::ostream& operator<<(std::ostream&, const T&)
    template <typename T>
        requires (!std::is_integral_v<T> &&
                  !std::is_floating_point_v<T> &&
                  !std::is_same_v<T, std::string_view> &&
                  !std::is_same_v<T, const char*> &&
                  !std::is_same_v<T, char> &&
                  !std::is_same_v<T, bool> &&
                  !std::is_same_v<T, const void*> &&
                  !std::is_same_v<T, LogLevel>)
    LogStream& operator<<(const T& val) {
        static_cast<std::ostream&>(*this) << val;
        return *this;
    }

    // Support std::endl and other manipulators
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
        manip(*this);
        return *this;
    }

    std::string_view view() const noexcept {
        return streambuf_.view();
    }

    size_t size() const noexcept {
        return streambuf_.size();
    }

    void reset() noexcept {
        streambuf_.reset();
        clear();
    }

    void write_header(const ReflectedContext& ctx, LogLevel level, bool emit_method, bool emit_thread_id = true) {
        // Date-time upto microsecond (26 chars)
        char ts[Timestamp::FORMATTED_LENGTH + 1];
        Timestamp::format_now(ts, sizeof(ts));
        write_fast(ts, Timestamp::FORMATTED_LENGTH);

        write_fast(" - ", 3);
        std::string_view lvl_str = to_string(level);
        write_fast(lvl_str.data(), lvl_str.size());

        if (emit_thread_id) {
            write_fast(" - ", 3);
            std::string_view tid_str = ThreadId::get_formatted();
            write_fast(tid_str.data(), tid_str.size());
        }

        write_fast(" - ", 3);

        write_fast(ctx.file_name.data(), ctx.file_name.size());
        write_fast(":", 1);

        // Linenumber
        char line_buf[16];
        auto [ptr, ec] = std::to_chars(line_buf, line_buf + sizeof(line_buf), ctx.line_number);
        if (ec == std::errc{}) {
            write_fast(line_buf, static_cast<size_t>(ptr - line_buf));
        }

        write_fast(" : ", 3);

        if (emit_method && !ctx.method_name.empty()) {
            write_fast("[", 1);
            if (!ctx.class_name.empty()) {
                write_fast(ctx.class_name.data(), ctx.class_name.size());
                write_fast("::", 2);
            }
            write_fast(ctx.method_name.data(), ctx.method_name.size());
            write_fast("] ", 2);
        }
    }

private:
    void write_fast(const char* data, size_t len) noexcept {
        streambuf_.sputn(data, static_cast<std::streamsize>(len));
    }

    char buffer_[BufferSize];
    StackStreamBuf streambuf_;
};

} // namespace vordr
