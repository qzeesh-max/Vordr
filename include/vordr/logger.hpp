// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <shared_mutex>
#include <meta>
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

    uint64_t config_epoch() const noexcept {
        return config_.epoch();
    }

    LogLevel get_effective_level_uncached(const ReflectedContext& ctx) const noexcept {
        return config_.get_effective_level(ctx);
    }

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

template <std::meta::info Scope, uint32_t Line>
struct CallSiteStorage {
    static inline std::atomic<uint64_t> cached_epoch{0};
    static inline std::atomic<uint8_t> cached_level{static_cast<uint8_t>(LogLevel::INFO)};

    static bool is_enabled(LogLevel lvl, const ReflectedContext& ctx) noexcept {
        if (lvl == LogLevel::OFF) [[unlikely]] return false;

        uint64_t g_epoch = Logger::instance().config_epoch();
        uint64_t c_epoch = cached_epoch.load(std::memory_order_relaxed);
        if (__builtin_expect(c_epoch != g_epoch, 0)) {
            resolve(g_epoch, ctx);
        }

        uint8_t eff = cached_level.load(std::memory_order_relaxed);
        if (eff == static_cast<uint8_t>(LogLevel::OFF)) [[unlikely]] return false;
        return static_cast<uint8_t>(lvl) >= eff;
    }

    static void resolve(uint64_t g_epoch, const ReflectedContext& ctx) noexcept {
        LogLevel effective = Logger::instance().get_effective_level_uncached(ctx);
        cached_level.store(static_cast<uint8_t>(effective), std::memory_order_relaxed);
        cached_epoch.store(g_epoch, std::memory_order_release);
    }
};

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
