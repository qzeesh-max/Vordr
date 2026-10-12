// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <meta>
#include <string_view>
#include "vordr/types.hpp"

namespace vordr::detail {

consteval ReflectedContext extract_reflected_context_impl(
    std::meta::info scope,
    std::string_view file,
    uint32_t line)
{
    ReflectedContext ctx{};
    ctx.file_name = strip_file_path(file);
    ctx.line_number = line;
    ctx.full_scope_name = std::meta::display_string_of(scope);

    auto curr = scope;
    while (curr != std::meta::info{}) {
        std::string_view dname = std::meta::display_string_of(curr);
        if (dname.find("<lambda") != std::string_view::npos) {
            if (!std::meta::has_parent(curr)) {
                break;
            }
            auto p = std::meta::parent_of(curr);
            if (std::meta::display_string_of(p).find("<lambda") != std::string_view::npos) {
                if (!std::meta::has_parent(p)) {
                    break;
                }
                curr = std::meta::parent_of(p);
            } else {
                curr = p;
            }
            continue;
        }

        if (std::meta::is_function(curr)) {
            if (ctx.method_name.empty() && std::meta::has_identifier(curr)) {
                ctx.method_name = std::meta::identifier_of(curr);
            }

            if (!std::meta::has_parent(curr)) {
                break;
            }
            auto p = std::meta::parent_of(curr);
            if (std::meta::is_type(p) && std::meta::is_class_type(p)) {
                std::string_view pname = std::meta::display_string_of(p);
                if (pname.find("<lambda") == std::string_view::npos) {
                    ctx.class_name = pname;
                    break;
                }
            }
        } else if (std::meta::is_type(curr) && std::meta::is_class_type(curr)) {
            if (dname.find("<lambda") == std::string_view::npos) {
                if (ctx.class_name.empty()) {
                    ctx.class_name = dname;
                }
                break;
            }
        }

        if (!std::meta::has_parent(curr)) {
            break;
        }
        curr = std::meta::parent_of(curr);
    }

    return ctx;
}

template <std::meta::info Scope>
consteval ReflectedContext get_context(std::string_view file, uint32_t line) {
    return extract_reflected_context_impl(Scope, file, line);
}

consteval uint64_t compute_call_site_id(const ReflectedContext& ctx) {
    uint64_t h = 14695981039346656037ULL;
    for (char c : ctx.file_name) {
        h = (h ^ static_cast<uint64_t>(c)) * 1099511628211ULL;
    }
    for (char c : ctx.class_name) {
        h = (h ^ static_cast<uint64_t>(c)) * 1099511628211ULL;
    }
    for (char c : ctx.method_name) {
        h = (h ^ static_cast<uint64_t>(c)) * 1099511628211ULL;
    }
    for (char c : ctx.full_scope_name) {
        h = (h ^ static_cast<uint64_t>(c)) * 1099511628211ULL;
    }
    h = (h ^ static_cast<uint64_t>(ctx.line_number)) * 1099511628211ULL;
    return h;
}

} // namespace vordr::detail

