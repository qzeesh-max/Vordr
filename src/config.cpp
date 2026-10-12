// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/config.hpp"
#include <mutex>
#include <algorithm>

namespace vordr {

namespace {

bool matches_qualified(std::string_view rule_name, std::string_view class_name, std::string_view method_name) noexcept {
    if (class_name.empty() || method_name.empty()) return false;
    size_t expected_len = class_name.size() + 2 + method_name.size();
    if (rule_name.size() != expected_len) return false;
    if (!rule_name.starts_with(class_name)) return false;
    if (rule_name.substr(class_name.size(), 2) != "::") return false;
    return rule_name.ends_with(method_name);
}

std::string_view get_unqualified_name(std::string_view full_name) noexcept {
    size_t last_colons = full_name.rfind("::");
    if (last_colons != std::string_view::npos && last_colons + 2 < full_name.size()) {
        return full_name.substr(last_colons + 2);
    }
    return full_name;
}

} // namespace

Config::Config(const Config& other) {
    std::shared_lock lock(other.mutex_);
    epoch_.store(other.epoch_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    central_level_ = other.central_level_;
    emit_method_ = other.emit_method_;
    sink_path_ = other.sink_path_;
    sink_max_size_ = other.sink_max_size_;
    method_rules_ = other.method_rules_;
    class_rules_ = other.class_rules_;
    file_rules_ = other.file_rules_;
}

Config::Config(Config&& other) noexcept {
    std::unique_lock lock(other.mutex_);
    epoch_.store(other.epoch_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    central_level_ = other.central_level_;
    emit_method_ = other.emit_method_;
    sink_path_ = std::move(other.sink_path_);
    sink_max_size_ = other.sink_max_size_;
    method_rules_ = std::move(other.method_rules_);
    class_rules_ = std::move(other.class_rules_);
    file_rules_ = std::move(other.file_rules_);
}

Config& Config::operator=(const Config& other) {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::shared_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        epoch_.fetch_add(1, std::memory_order_release);
        central_level_ = other.central_level_;
        emit_method_ = other.emit_method_;
        sink_path_ = other.sink_path_;
        sink_max_size_ = other.sink_max_size_;
        method_rules_ = other.method_rules_;
        class_rules_ = other.class_rules_;
        file_rules_ = other.file_rules_;
    }
    return *this;
}

Config& Config::operator=(Config&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::unique_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        epoch_.fetch_add(1, std::memory_order_release);
        central_level_ = other.central_level_;
        emit_method_ = other.emit_method_;
        sink_path_ = std::move(other.sink_path_);
        sink_max_size_ = other.sink_max_size_;
        method_rules_ = std::move(other.method_rules_);
        class_rules_ = std::move(other.class_rules_);
        file_rules_ = std::move(other.file_rules_);
    }
    return *this;
}

void Config::set_central_level(LogLevel level) noexcept {
    std::unique_lock lock(mutex_);
    central_level_ = level;
    epoch_.fetch_add(1, std::memory_order_release);
}

LogLevel Config::central_level() const noexcept {
    std::shared_lock lock(mutex_);
    return central_level_;
}

void Config::set_emit_method(bool emit) noexcept {
    std::unique_lock lock(mutex_);
    emit_method_ = emit;
    epoch_.fetch_add(1, std::memory_order_release);
}

bool Config::emit_method() const noexcept {
    std::shared_lock lock(mutex_);
    return emit_method_;
}

void Config::set_sink_path(std::string path) {
    std::unique_lock lock(mutex_);
    sink_path_ = std::move(path);
    epoch_.fetch_add(1, std::memory_order_release);
}

const std::string& Config::sink_path() const noexcept {
    std::shared_lock lock(mutex_);
    return sink_path_;
}

void Config::set_sink_max_size(size_t max_size) noexcept {
    std::unique_lock lock(mutex_);
    sink_max_size_ = max_size;
    epoch_.fetch_add(1, std::memory_order_release);
}

size_t Config::sink_max_size() const noexcept {
    std::shared_lock lock(mutex_);
    return sink_max_size_;
}

void Config::set_file_level(std::string_view file_name, LogLevel level) {
    std::unique_lock lock(mutex_);
    std::string_view stripped = strip_file_path(file_name);
    for (auto& r : file_rules_) {
        if (r.name == stripped) {
            r.level = level;
            epoch_.fetch_add(1, std::memory_order_release);
            return;
        }
    }
    file_rules_.push_back({std::string(stripped), level});
    epoch_.fetch_add(1, std::memory_order_release);
}

void Config::set_class_level(std::string_view class_name, LogLevel level) {
    std::unique_lock lock(mutex_);
    for (auto& r : class_rules_) {
        if (r.name == class_name) {
            r.level = level;
            epoch_.fetch_add(1, std::memory_order_release);
            return;
        }
    }
    class_rules_.push_back({std::string(class_name), level});
    epoch_.fetch_add(1, std::memory_order_release);
}

void Config::set_method_level(std::string_view method_name, LogLevel level) {
    std::unique_lock lock(mutex_);
    for (auto& r : method_rules_) {
        if (r.name == method_name) {
            r.level = level;
            epoch_.fetch_add(1, std::memory_order_release);
            return;
        }
    }
    method_rules_.push_back({std::string(method_name), level});
    epoch_.fetch_add(1, std::memory_order_release);
}

void Config::clear_overrides() {
    std::unique_lock lock(mutex_);
    file_rules_.clear();
    class_rules_.clear();
    method_rules_.clear();
    epoch_.fetch_add(1, std::memory_order_release);
}

LogLevel Config::get_effective_level(const ReflectedContext& ctx) const noexcept {
    return get_effective_level(ctx.file_name, ctx.class_name, ctx.method_name);
}

LogLevel Config::get_effective_level(std::string_view file,
                                     std::string_view class_name,
                                     std::string_view method_name) const noexcept
{
    std::shared_lock lock(mutex_);

    // 1. Method-level override: zero-allocation matching
    if (!method_name.empty()) {
        for (const auto& r : method_rules_) {
            if (matches_qualified(r.name, class_name, method_name)) {
                return r.level;
            }
            if (r.name == method_name) {
                return r.level;
            }
        }
    }

    // 2. Class-level override: zero-allocation matching
    if (!class_name.empty()) {
        std::string_view unqual = get_unqualified_name(class_name);
        for (const auto& r : class_rules_) {
            if (r.name == class_name || r.name == unqual) {
                return r.level;
            }
        }
    }

    // 3. File-level override: zero-allocation matching
    if (!file.empty()) {
        std::string_view stripped = strip_file_path(file);
        for (const auto& r : file_rules_) {
            if (r.name == file || r.name == stripped) {
                return r.level;
            }
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
