// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include "vordr/reflection.hpp"
#include "vordr/logger.hpp"

#define VORDR_LOG(level) \
    if (constexpr auto _vordr_ctx = ::vordr::detail::get_context<::std::meta::access_context::current().scope()>(__FILE__, __LINE__); \
        !::vordr::Logger::instance().is_enabled(level, _vordr_ctx)) ; \
    else ::vordr::detail::LogStatement(level, _vordr_ctx, ::vordr::Logger::instance())

#define VORDR_TRACE VORDR_LOG(::vordr::LogLevel::TRACE)
#define VORDR_DEBUG VORDR_LOG(::vordr::LogLevel::DEBUG)
#define VORDR_INFO  VORDR_LOG(::vordr::LogLevel::INFO)
#define VORDR_WARN  VORDR_LOG(::vordr::LogLevel::WARN)
#define VORDR_ERROR VORDR_LOG(::vordr::LogLevel::ERROR)
#define VORDR_FATAL VORDR_LOG(::vordr::LogLevel::FATAL)
