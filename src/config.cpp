// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/config.hpp"
#include <mutex>

namespace vordr {

Config::Config(const Config& other) {
    std::shared_lock lock(other.mutex_);
    central_level_ = other.central_level_;
    emit_method_ = other.emit_method_;
    sink_path_ = other.sink_path_;
    sink_max_size_ = other.sink_max_size_;
    file_levels_ = other.file_levels_;
    class_levels_ = other.class_levels_;
    method_levels_ = other.method_levels_;
}

Config::Config(Config&& other) noexcept {
    std::unique_lock lock(other.mutex_);
    central_level_ = other.central_level_;
    emit_method_ = other.emit_method_;
    sink_path_ = std::move(other.sink_path_);
    sink_max_size_ = other.sink_max_size_;
    file_levels_ = std::move(other.file_levels_);
    class_levels_ = std::move(other.class_levels_);
    method_levels_ = std::move(other.method_levels_);
}

Config& Config::operator=(const Config& other) {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::shared_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        central_level_ = other.central_level_;
        emit_method_ = other.emit_method_;
        sink_path_ = other.sink_path_;
        sink_max_size_ = other.sink_max_size_;
        file_levels_ = other.file_levels_;
        class_levels_ = other.class_levels_;
        method_levels_ = other.method_levels_;
    }
    return *this;
}

Config& Config::operator=(Config&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::unique_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        central_level_ = other.central_level_;
        emit_method_ = other.emit_method_;
        sink_path_ = std::move(other.sink_path_);
        sink_max_size_ = other.sink_max_size_;
        file_levels_ = std::move(other.file_levels_);
        class_levels_ = std::move(other.class_levels_);
        method_levels_ = std::move(other.method_levels_);
    }
    return *this;
}

void Config::set_central_level(LogLevel level) noexcept {
    std::unique_lock lock(mutex_);
    central_level_ = level;
}

LogLevel Config::central_level() const noexcept {
    std::shared_lock lock(mutex_);
    return central_level_;
}

void Config::set_emit_method(bool emit) noexcept {
    std::unique_lock lock(mutex_);
    emit_method_ = emit;
}

bool Config::emit_method() const noexcept {
    std::shared_lock lock(mutex_);
    return emit_method_;
}

void Config::set_sink_path(std::string path) {
    std::unique_lock lock(mutex_);
    sink_path_ = std::move(path);
}

const std::string& Config::sink_path() const noexcept {
    std::shared_lock lock(mutex_);
    return sink_path_;
}

void Config::set_sink_max_size(size_t max_size) noexcept {
    std::unique_lock lock(mutex_);
    sink_max_size_ = max_size;
}

size_t Config::sink_max_size() const noexcept {
    std::shared_lock lock(mutex_);
    return sink_max_size_;
}

void Config::set_file_level(std::string_view file_name, LogLevel level) {
    std::unique_lock lock(mutex_);
    std::string_view stripped = strip_file_path(file_name);
    file_levels_[std::string(stripped)] = level;
}

void Config::set_class_level(std::string_view class_name, LogLevel level) {
    std::unique_lock lock(mutex_);
    class_levels_[std::string(class_name)] = level;
}

void Config::set_method_level(std::string_view method_name, LogLevel level) {
    std::unique_lock lock(mutex_);
    method_levels_[std::string(method_name)] = level;
}

void Config::clear_overrides() {
    std::unique_lock lock(mutex_);
    file_levels_.clear();
    class_levels_.clear();
    method_levels_.clear();
}

LogLevel Config::get_effective_level(const ReflectedContext& ctx) const noexcept {
    return get_effective_level(ctx.file_name, ctx.class_name, ctx.method_name);
}

LogLevel Config::get_effective_level(std::string_view file,
                                     std::string_view class_name,
                                     std::string_view method_name) const noexcept
{
    std::shared_lock lock(mutex_);

    // 1. Method-level override: check qualified class_name::method_name, then bare method_name
    if (!method_name.empty()) {
        if (!class_name.empty()) {
            std::string qualified;
            qualified.reserve(class_name.size() + 2 + method_name.size());
            qualified.append(class_name).append("::").append(method_name);
            auto it = method_levels_.find(qualified);
            if (it != method_levels_.end()) {
                return it->second;
            }
        }
        auto it = method_levels_.find(std::string(method_name));
        if (it != method_levels_.end()) {
            return it->second;
        }
    }

    // 2. Class-level override: check full class_name, then unqualified class_name
    if (!class_name.empty()) {
        auto it = class_levels_.find(std::string(class_name));
        if (it != class_levels_.end()) {
            return it->second;
        }
        size_t last_colons = class_name.rfind("::");
        if (last_colons != std::string_view::npos && last_colons + 2 < class_name.size()) {
            std::string_view unqual = class_name.substr(last_colons + 2);
            it = class_levels_.find(std::string(unqual));
            if (it != class_levels_.end()) {
                return it->second;
            }
        }
    }

    // 3. File-level override
    if (!file.empty()) {
        std::string_view stripped = strip_file_path(file);
        auto it = file_levels_.find(std::string(stripped));
        if (it != file_levels_.end()) {
            return it->second;
        }
    }

    // 4. Fallback to central level
    return central_level_;
}

bool Config::is_enabled(LogLevel level, const ReflectedContext& ctx) const noexcept {
    return is_enabled(level, ctx.file_name, ctx.class_name, ctx.method_name);
}

bool Config::is_enabled(LogLevel level,
                        std::string_view file,
                        std::string_view class_name,
                        std::string_view method_name) const noexcept
{
    if (level == LogLevel::OFF) [[unlikely]] {
        return false;
    }
    LogLevel effective = get_effective_level(file, class_name, method_name);
    if (effective == LogLevel::OFF) [[unlikely]] {
        return false;
    }

    if (level == LogLevel::FATAL || level == LogLevel::ERROR || level == LogLevel::WARN) [[likely]] {
        if (static_cast<uint8_t>(level) >= static_cast<uint8_t>(effective)) [[likely]] {
            return true;
        }
        return false;
    } else if (level == LogLevel::TRACE) [[unlikely]] {
        if (static_cast<uint8_t>(level) >= static_cast<uint8_t>(effective)) [[unlikely]] {
            return true;
        }
        return false;
    }

    return static_cast<uint8_t>(level) >= static_cast<uint8_t>(effective);
}

} // namespace vordr
