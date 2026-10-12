// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <shared_mutex>
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

    void set_sink_path(std::string path);
    const std::string& sink_path() const noexcept;

    void set_sink_max_size(size_t max_size) noexcept;
    size_t sink_max_size() const noexcept;

    void set_file_level(std::string_view file_name, LogLevel level);
    void set_class_level(std::string_view class_name, LogLevel level);
    void set_method_level(std::string_view method_name, LogLevel level);

    void clear_overrides();

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
    mutable std::shared_mutex mutex_;
    LogLevel central_level_{LogLevel::INFO};
    bool emit_method_{false};
    std::string sink_path_{"vordr.log"};
    size_t sink_max_size_{64 * 1024 * 1024}; // 64MB default

    std::unordered_map<std::string, LogLevel> file_levels_;
    std::unordered_map<std::string, LogLevel> class_levels_;
    std::unordered_map<std::string, LogLevel> method_levels_;
};

} // namespace vordr
