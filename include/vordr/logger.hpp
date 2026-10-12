// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <shared_mutex>
#include "vordr/types.hpp"
#include "vordr/config.hpp"
#include "vordr/mmap_sink.hpp"
#include "vordr/stream_buffer.hpp"

namespace vordr {

class Logger {
public:
    static Logger& instance() noexcept;

    bool init_xml(const std::string& xml_path, std::string* error_msg = nullptr);
    bool init_xml_string(std::string_view xml_content, std::string* error_msg = nullptr);
    bool init(Config config);

    void set_sink(std::unique_ptr<MmapSink> sink) noexcept;
    MmapSink* sink() noexcept;

    Config& config() noexcept { return config_; }
    const Config& config() const noexcept { return config_; }

    bool is_enabled(LogLevel level, const ReflectedContext& ctx) const noexcept;

    void write_log(std::string_view entry) noexcept;

    void flush(bool sync = false) noexcept;
    void close() noexcept;

    // Reset logger state (useful for test isolation)
    void reset() noexcept;

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    Config config_;
    std::unique_ptr<MmapSink> sink_;
    mutable std::shared_mutex sink_mutex_;
};

namespace detail {

class LogStatement {
public:
    LogStatement(LogLevel level, const ReflectedContext& ctx, Logger& logger) noexcept
        : logger_(logger)
    {
        stream_.write_header(ctx, level, logger.config().emit_method());
    }

    ~LogStatement() {
        stream_ << '\n';
        logger_.write_log(stream_.view());
    }

    // Forwarding operators to LogStream
    template <typename T>
    LogStatement& operator<<(const T& val) {
        stream_ << val;
        return *this;
    }

    LogStatement& operator<<(std::ostream& (*manip)(std::ostream&)) {
        stream_ << manip;
        return *this;
    }

private:
    Logger& logger_;
    LogStream<4096> stream_;
};

} // namespace detail

} // namespace vordr
