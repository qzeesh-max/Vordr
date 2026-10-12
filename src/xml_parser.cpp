// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/xml_parser.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <charconv>

namespace vordr {

namespace {

void skip_whitespace(std::string_view xml, size_t& pos) {
    while (pos < xml.size() && std::isspace(static_cast<unsigned char>(xml[pos]))) {
        ++pos;
    }
}

bool skip_comment_or_pi(std::string_view xml, size_t& pos) {
    if (pos + 4 <= xml.size() && xml.substr(pos, 4) == "<!--") {
        size_t end = xml.find("-->", pos + 4);
        if (end == std::string_view::npos) {
            pos = xml.size();
        } else {
            pos = end + 3;
        }
        return true;
    }
    if (pos + 2 <= xml.size() && xml.substr(pos, 2) == "<?") {
        size_t end = xml.find("?>", pos + 2);
        if (end == std::string_view::npos) {
            pos = xml.size();
        } else {
            pos = end + 2;
        }
        return true;
    }
    return false;
}

std::string parse_attr_value(std::string_view xml, size_t& pos) {
    if (pos >= xml.size()) return "";
    char quote = xml[pos];
    if (quote != '"' && quote != '\'') return "";
    ++pos;
    size_t start = pos;
    while (pos < xml.size() && xml[pos] != quote) {
        ++pos;
    }
    std::string val(xml.substr(start, pos - start));
    if (pos < xml.size() && xml[pos] == quote) {
        ++pos;
    }
    return val;
}

bool parse_xml_node(std::string_view xml, size_t& pos, XmlElement& out_elem, std::string* error_msg) {
    while (pos < xml.size()) {
        skip_whitespace(xml, pos);
        if (pos >= xml.size()) return false;
        if (skip_comment_or_pi(xml, pos)) continue;
        break;
    }
    if (pos >= xml.size() || xml[pos] != '<') {
        if (error_msg) *error_msg = "Expected '<' at position " + std::to_string(pos);
        return false;
    }

    ++pos; // skip '<'
    if (pos >= xml.size() || xml[pos] == '/') {
        if (error_msg) *error_msg = "Unexpected closing tag at position " + std::to_string(pos);
        return false;
    }

    // Parse tag name
    size_t tag_start = pos;
    while (pos < xml.size() && !std::isspace(static_cast<unsigned char>(xml[pos])) &&
           xml[pos] != '>' && xml[pos] != '/') {
        ++pos;
    }
    out_elem.tag = std::string(xml.substr(tag_start, pos - tag_start));

    // Parse attributes
    while (pos < xml.size()) {
        skip_whitespace(xml, pos);
        if (pos >= xml.size()) break;
        if (xml[pos] == '>' || xml[pos] == '/') break;

        size_t attr_name_start = pos;
        while (pos < xml.size() && !std::isspace(static_cast<unsigned char>(xml[pos])) &&
               xml[pos] != '=' && xml[pos] != '>' && xml[pos] != '/') {
            ++pos;
        }
        std::string attr_name(xml.substr(attr_name_start, pos - attr_name_start));
        skip_whitespace(xml, pos);

        if (pos < xml.size() && xml[pos] == '=') {
            ++pos;
            skip_whitespace(xml, pos);
            std::string attr_val = parse_attr_value(xml, pos);
            out_elem.attributes[attr_name] = std::move(attr_val);
        } else {
            out_elem.attributes[attr_name] = "";
        }
    }

    skip_whitespace(xml, pos);
    if (pos < xml.size() && xml[pos] == '/') {
        ++pos;
        if (pos < xml.size() && xml[pos] == '>') {
            ++pos;
            return true; // self-closing
        }
        if (error_msg) *error_msg = "Expected '>' after '/' in self-closing tag " + out_elem.tag;
        return false;
    }

    if (pos < xml.size() && xml[pos] == '>') {
        ++pos;
    } else {
        if (error_msg) *error_msg = "Expected '>' in tag " + out_elem.tag;
        return false;
    }

    // Parse children or text until </tag>
    while (pos < xml.size()) {
        skip_whitespace(xml, pos);
        if (pos >= xml.size()) break;

        if (skip_comment_or_pi(xml, pos)) continue;

        if (pos + 1 < xml.size() && xml[pos] == '<' && xml[pos + 1] == '/') {
            pos += 2;
            size_t close_start = pos;
            while (pos < xml.size() && xml[pos] != '>') {
                ++pos;
            }
            std::string_view close_tag = xml.substr(close_start, pos - close_start);
            // Trim whitespace
            while (!close_tag.empty() && std::isspace(static_cast<unsigned char>(close_tag.front()))) close_tag.remove_prefix(1);
            while (!close_tag.empty() && std::isspace(static_cast<unsigned char>(close_tag.back()))) close_tag.remove_suffix(1);

            if (close_tag != out_elem.tag) {
                if (error_msg) *error_msg = "Mismatched closing tag: expected </" + out_elem.tag + ">, got </" + std::string(close_tag) + ">";
                return false;
            }
            if (pos < xml.size() && xml[pos] == '>') {
                ++pos;
            }
            return true;
        }

        if (xml[pos] == '<') {
            XmlElement child;
            if (!parse_xml_node(xml, pos, child, error_msg)) {
                return false;
            }
            out_elem.children.push_back(std::move(child));
        } else {
            size_t text_start = pos;
            while (pos < xml.size() && xml[pos] != '<') {
                ++pos;
            }
            out_elem.text += xml.substr(text_start, pos - text_start);
        }
    }

    if (error_msg) *error_msg = "Unclosed tag: <" + out_elem.tag + ">";
    return false;
}

} // namespace

bool XmlConfigParser::parse_document(std::string_view xml, XmlElement& root, std::string* error_msg) {
    size_t pos = 0;
    while (pos < xml.size()) {
        skip_whitespace(xml, pos);
        if (pos >= xml.size()) break;
        if (skip_comment_or_pi(xml, pos)) continue;
        if (xml[pos] == '<') {
            return parse_xml_node(xml, pos, root, error_msg);
        }
        ++pos;
    }
    if (error_msg) *error_msg = "Empty XML document";
    return false;
}

bool XmlConfigParser::apply_element(const XmlElement& elem, Config& out_config, std::string* error_msg) {
    const std::string& tag = elem.tag;

    if (tag == "vordr" || tag == "configuration" || tag == "config") {
        if (auto lvl = elem.get_attr("level")) {
            out_config.set_central_level(level_from_string(*lvl));
        }
        if (auto em = elem.get_attr("emit_method")) {
            out_config.set_emit_method(*em == "true" || *em == "1" || *em == "yes");
        }
        if (auto et = elem.get_attr("emit_thread_id")) {
            out_config.set_emit_thread_id(*et == "true" || *et == "1" || *et == "yes");
        } else if (auto et2 = elem.get_attr("emit_thread")) {
            out_config.set_emit_thread_id(*et2 == "true" || *et2 == "1" || *et2 == "yes");
        }
        for (const auto& child : elem.children) {
            if (!apply_element(child, out_config, error_msg)) return false;
        }
        return true;
    }

    if (tag == "sink") {
        if (auto path = elem.get_attr("path")) {
            out_config.set_sink_path(*path);
        }
        if (auto sz_str = elem.get_attr("max_size")) {
            size_t sz = 0;
            auto [p, ec] = std::from_chars(sz_str->data(), sz_str->data() + sz_str->size(), sz);
            if (ec == std::errc{} && sz > 0) {
                out_config.set_sink_max_size(sz);
            }
        }
        if (auto app = elem.get_attr("append")) {
            out_config.set_sink_append(*app == "true" || *app == "1" || *app == "yes");
        }
        return true;
    }

    if (tag == "logging" || tag == "central" || tag == "root") {
        if (auto lvl = elem.get_attr("level")) {
            out_config.set_central_level(level_from_string(*lvl));
        }
        if (auto em = elem.get_attr("emit_method")) {
            out_config.set_emit_method(*em == "true" || *em == "1" || *em == "yes");
        }
        if (auto et = elem.get_attr("emit_thread_id")) {
            out_config.set_emit_thread_id(*et == "true" || *et == "1" || *et == "yes");
        } else if (auto et2 = elem.get_attr("emit_thread")) {
            out_config.set_emit_thread_id(*et2 == "true" || *et2 == "1" || *et2 == "yes");
        }
        for (const auto& child : elem.children) {
            if (!apply_element(child, out_config, error_msg)) return false;
        }
        return true;
    }

    if (tag == "file") {
        auto name = elem.get_attr("name");
        auto lvl = elem.get_attr("level");
        if (!name || !lvl) {
            if (error_msg) *error_msg = "<file> tag requires 'name' and 'level' attributes";
            return false;
        }
        out_config.set_file_level(*name, level_from_string(*lvl));
        return true;
    }

    if (tag == "class") {
        auto name = elem.get_attr("name");
        auto lvl = elem.get_attr("level");
        if (!name || !lvl) {
            if (error_msg) *error_msg = "<class> tag requires 'name' and 'level' attributes";
            return false;
        }
        out_config.set_class_level(*name, level_from_string(*lvl));
        return true;
    }

    if (tag == "method") {
        auto name = elem.get_attr("name");
        auto lvl = elem.get_attr("level");
        if (!name || !lvl) {
            if (error_msg) *error_msg = "<method> tag requires 'name' and 'level' attributes";
            return false;
        }
        out_config.set_method_level(*name, level_from_string(*lvl));
        return true;
    }

    // For any wrapper tags like <overrides> or <categories>
    for (const auto& child : elem.children) {
        if (!apply_element(child, out_config, error_msg)) return false;
    }
    return true;
}

bool XmlConfigParser::parse_string(std::string_view xml_content, Config& out_config, std::string* error_msg) {
    XmlElement root;
    if (!parse_document(xml_content, root, error_msg)) {
        return false;
    }
    return apply_element(root, out_config, error_msg);
}

bool XmlConfigParser::parse_file(const std::string& file_path, Config& out_config, std::string* error_msg) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        if (error_msg) *error_msg = "Could not open XML file: " + file_path;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parse_string(content, out_config, error_msg);
}

} // namespace vordr
