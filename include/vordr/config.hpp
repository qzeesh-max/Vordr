// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <cstdint>
#include "vordr/types.hpp"

namespace vordr {

class Config {
public:
    Config() = default;
    Config(const Config& other);
    Config(Config&& other) noexcept;
    Config& operator=(const Config& other);
    Config& operator=(Config&& other) noexcept;

    void set_central_level(LogLevel level) noexcept;
    LogLevel central_level() const noexcept;

    void set_emit_method(bool emit) noexcept;
    bool emit_method() const noexcept;

    void set_emit_thread_id(bool emit) noexcept;
    bool emit_thread_id() const noexcept;

    void set_sink_path(std::string path);
    const std::string& sink_path() const noexcept;

    void set_sink_max_size(size_t max_size) noexcept;
    size_t sink_max_size() const noexcept;

    void set_sink_append(bool append) noexcept;
    bool sink_append() const noexcept;

    void set_file_level(std::string_view file_name, LogLevel level);
    void set_class_level(std::string_view class_name, LogLevel level);
    void set_method_level(std::string_view method_name, LogLevel level);

    void clear_overrides();

    uint64_t epoch() const noexcept {
        return epoch_.load(std::memory_order_acquire);
    }

    LogLevel get_effective_level(const ReflectedContext& ctx) const noexcept;
    LogLevel get_effective_level(std::string_view file,
                                 std::string_view class_name,
                                 std::string_view method_name) const noexcept;

    bool is_enabled(LogLevel level, const ReflectedContext& ctx) const noexcept;
    bool is_enabled(LogLevel level,
                    std::string_view file,
                    std::string_view class_name,
                    std::string_view method_name) const noexcept;

private:
    struct Rule {
        std::string name;
        LogLevel level;
    };

    mutable std::shared_mutex mutex_;
    std::atomic<uint64_t> epoch_{1};
    LogLevel central_level_{LogLevel::INFO};
    bool emit_method_{false};
    bool emit_thread_id_{true};
    std::string sink_path_{"vordr.log"};
    size_t sink_max_size_{64 * 1024 * 1024}; // 64MB default
    bool sink_append_{true};

    std::vector<Rule> method_rules_;
    std::vector<Rule> class_rules_;
    std::vector<Rule> file_rules_;
};

} // namespace vordr
