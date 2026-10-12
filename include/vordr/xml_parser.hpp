// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include "vordr/config.hpp"

namespace vordr {

struct XmlAttribute {
    std::string name;
    std::string value;
};

struct XmlElement {
    std::string tag;
    std::unordered_map<std::string, std::string> attributes;
    std::string text;
    std::vector<XmlElement> children;

    const std::string* get_attr(const std::string& name) const {
        auto it = attributes.find(name);
        if (it != attributes.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

class XmlConfigParser {
public:
    static bool parse_string(std::string_view xml_content, Config& out_config, std::string* error_msg = nullptr);
    static bool parse_file(const std::string& file_path, Config& out_config, std::string* error_msg = nullptr);

    // Low-level XML document parser
    static bool parse_document(std::string_view xml, XmlElement& root, std::string* error_msg = nullptr);

private:
    static bool apply_element(const XmlElement& elem, Config& out_config, std::string* error_msg);
};

} // namespace vordr
