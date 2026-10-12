// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/logger.hpp"
#include "vordr/xml_parser.hpp"

namespace vordr {

Logger& Logger::instance() noexcept {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    // Sink is lazily initialized on first write or explicit configuration
}

Logger::~Logger() {
    close();
}

bool Logger::init_xml(const std::string& xml_path, std::string* error_msg) {
    Config new_cfg;
    if (!XmlConfigParser::parse_file(xml_path, new_cfg, error_msg)) {
        return false;
    }
    return init(std::move(new_cfg));
}

bool Logger::init_xml_string(std::string_view xml_content, std::string* error_msg) {
    Config new_cfg;
    if (!XmlConfigParser::parse_string(xml_content, new_cfg, error_msg)) {
        return false;
    }
    return init(std::move(new_cfg));
}

bool Logger::init(Config config) {
    std::unique_lock lock(sink_mutex_);
    if (sink_) {
        sink_->close();
        sink_.reset();
    }
    config_ = std::move(config);
    sink_ = std::make_unique<MmapSink>(config_.sink_path(), config_.sink_max_size(), config_.sink_append());
    return sink_->is_open();
}

void Logger::set_sink(std::unique_ptr<MmapSink> sink) noexcept {
    std::unique_lock lock(sink_mutex_);
    if (sink_) {
        sink_->close();
    }
    sink_ = std::move(sink);
}

MmapSink* Logger::sink() noexcept {
    std::shared_lock lock(sink_mutex_);
    if (!sink_) {
        lock.unlock();
        std::unique_lock ulock(sink_mutex_);
        if (!sink_) {
            sink_ = std::make_unique<MmapSink>(config_.sink_path(), config_.sink_max_size(), config_.sink_append());
        }
        return sink_.get();
    }
    return sink_.get();
}

bool Logger::is_enabled(LogLevel level, const ReflectedContext& ctx) const noexcept {
    if (level == LogLevel::FATAL || level == LogLevel::ERROR || level == LogLevel::WARN) [[likely]] {
        return config_.is_enabled(level, ctx);
    } else if (level == LogLevel::TRACE) [[unlikely]] {
        return config_.is_enabled(level, ctx);
    }
    return config_.is_enabled(level, ctx);
}

void Logger::write_log(std::string_view entry) noexcept {
    std::shared_lock lock(sink_mutex_);
    if (!sink_) {
        lock.unlock();
        std::unique_lock ulock(sink_mutex_);
        if (!sink_) {
            sink_ = std::make_unique<MmapSink>(config_.sink_path(), config_.sink_max_size(), config_.sink_append());
        }
        if (sink_) {
            sink_->write(entry);
        }
        return;
    }
    sink_->write(entry);
}

void Logger::flush(bool sync) noexcept {
    std::shared_lock lock(sink_mutex_);
    if (sink_) {
        sink_->flush(sync);
    }
}

void Logger::close() noexcept {
    std::unique_lock lock(sink_mutex_);
    if (sink_) {
        sink_->close();
        sink_.reset();
    }
}

void Logger::reset() noexcept {
    std::unique_lock lock(sink_mutex_);
    if (sink_) {
        sink_->close();
        sink_.reset();
    }
    config_ = Config{};
}

} // namespace vordr
