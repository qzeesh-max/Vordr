// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <algorithm>
#include <ostream>

namespace vordr {

enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5,
    OFF   = 6
};

constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::OFF:   return "OFF";
    }
    return "UNKNOWN";
}

template <typename CharT, typename Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, LogLevel level) {
    return os << to_string(level);
}

constexpr LogLevel level_from_string(std::string_view str, LogLevel fallback = LogLevel::INFO) noexcept {
    if (str == "TRACE" || str == "trace") return LogLevel::TRACE;
    if (str == "DEBUG" || str == "debug") return LogLevel::DEBUG;
    if (str == "INFO"  || str == "info")  return LogLevel::INFO;
    if (str == "WARN"  || str == "warn" || str == "WARNING" || str == "warning")  return LogLevel::WARN;
    if (str == "ERROR" || str == "error") return LogLevel::ERROR;
    if (str == "FATAL" || str == "fatal") return LogLevel::FATAL;
    if (str == "OFF"   || str == "off")   return LogLevel::OFF;
    return fallback;
}

constexpr std::string_view strip_file_path(std::string_view path) noexcept {
    if (path.empty()) return path;
    size_t last_slash = path.rfind('/');
    if (last_slash == std::string_view::npos) {
        last_slash = path.rfind('\\');
    }
    if (last_slash != std::string_view::npos && last_slash + 1 < path.size()) {
        return path.substr(last_slash + 1);
    }
    return path;
}

struct ReflectedContext {
    std::string_view method_name{};
    std::string_view class_name{};
    std::string_view full_scope_name{};
    std::string_view file_name{};
    uint32_t line_number{0};
};

} // namespace vordr
