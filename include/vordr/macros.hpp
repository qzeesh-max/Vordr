// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include "vordr/reflection.hpp"
#include "vordr/logger.hpp"

// Branch-hinted log statements using compile-time reflection call-site storage:
// FATAL, ERROR, WARN are likely to be enabled in production environments -> [[likely]] branch
// TRACE is unlikely to be enabled in production environments -> [[unlikely]] branch
#define VORDR_LOG_LIKELY(level) \
    if (constexpr auto _vordr_ctx = ::vordr::detail::get_context<::std::meta::access_context::current().scope()>(__FILE__, __LINE__); \
        !::vordr::detail::CallSiteStorage<::vordr::detail::compute_call_site_id(_vordr_ctx)>::is_enabled(level, _vordr_ctx)) [[unlikely]] ; \
    else [[likely]] ::vordr::detail::LogStatement(level, _vordr_ctx, ::vordr::Logger::instance())

#define VORDR_LOG_UNLIKELY(level) \
    if (constexpr auto _vordr_ctx = ::vordr::detail::get_context<::std::meta::access_context::current().scope()>(__FILE__, __LINE__); \
        !::vordr::detail::CallSiteStorage<::vordr::detail::compute_call_site_id(_vordr_ctx)>::is_enabled(level, _vordr_ctx)) [[likely]] ; \
    else [[unlikely]] ::vordr::detail::LogStatement(level, _vordr_ctx, ::vordr::Logger::instance())

#define VORDR_LOG_DEFAULT(level) \
    if (constexpr auto _vordr_ctx = ::vordr::detail::get_context<::std::meta::access_context::current().scope()>(__FILE__, __LINE__); \
        !::vordr::detail::CallSiteStorage<::vordr::detail::compute_call_site_id(_vordr_ctx)>::is_enabled(level, _vordr_ctx)) ; \
    else ::vordr::detail::LogStatement(level, _vordr_ctx, ::vordr::Logger::instance())

// Generic VORDR_LOG with dynamic/static level gating hints
#define VORDR_LOG(level) \
    if (constexpr auto _vordr_ctx = ::vordr::detail::get_context<::std::meta::access_context::current().scope()>(__FILE__, __LINE__); \
        ((level) == ::vordr::LogLevel::WARN || (level) == ::vordr::LogLevel::ERROR || (level) == ::vordr::LogLevel::FATAL) ? \
            __builtin_expect(!::vordr::detail::CallSiteStorage<::vordr::detail::compute_call_site_id(_vordr_ctx)>::is_enabled(level, _vordr_ctx), 0) : \
            (((level) == ::vordr::LogLevel::TRACE) ? \
                __builtin_expect(!::vordr::detail::CallSiteStorage<::vordr::detail::compute_call_site_id(_vordr_ctx)>::is_enabled(level, _vordr_ctx), 1) : \
                !::vordr::detail::CallSiteStorage<::vordr::detail::compute_call_site_id(_vordr_ctx)>::is_enabled(level, _vordr_ctx))) ; \
    else ::vordr::detail::LogStatement(level, _vordr_ctx, ::vordr::Logger::instance())

#define VORDR_TRACE VORDR_LOG_UNLIKELY(::vordr::LogLevel::TRACE)
#define VORDR_DEBUG VORDR_LOG_DEFAULT(::vordr::LogLevel::DEBUG)
#define VORDR_INFO  VORDR_LOG_DEFAULT(::vordr::LogLevel::INFO)
#define VORDR_WARN  VORDR_LOG_LIKELY(::vordr::LogLevel::WARN)
#define VORDR_ERROR VORDR_LOG_LIKELY(::vordr::LogLevel::ERROR)
#define VORDR_FATAL VORDR_LOG_LIKELY(::vordr::LogLevel::FATAL)
