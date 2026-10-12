// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/stream_buffer.hpp"
#include "vordr/timestamp.hpp"
#include <regex>

struct CustomUserType {
    int id;
    std::string name;
};

std::ostream& operator<<(std::ostream& os, const CustomUserType& u) {
    return os << "User{id=" << u.id << ", name='" << u.name << "'}";
}

TEST_CASE("Timestamp - Microsecond Formatting") {
    char buf[64];
    size_t len = vordr::Timestamp::format_now(buf, sizeof(buf));
    ASSERT_EQ(len, vordr::Timestamp::FORMATTED_LENGTH);

    std::string str(buf, len);
    // Format must match: YYYY-MM-DD HH:MM:SS.ffffff
    std::regex ts_regex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6}$)");
    ASSERT_TRUE(std::regex_match(str, ts_regex));
}

TEST_CASE("StreamBuffer - Primitive Types Streaming") {
    vordr::LogStream<1024> stream;
    stream << "Value: " << 42 << ", float: " << 3.14 << ", bool: " << true << ", str: " << std::string_view("hello");

    std::string_view res = stream.view();
    ASSERT_TRUE(res.find("Value: 42") != std::string_view::npos);
    ASSERT_TRUE(res.find("bool: true") != std::string_view::npos);
    ASSERT_TRUE(res.find("hello") != std::string_view::npos);
}

TEST_CASE("StreamBuffer - Custom Type Operator<< Compatibility") {
    vordr::LogStream<1024> stream;
    CustomUserType user{101, "Alice"};
    stream << "Logging " << user << " finished.";

    std::string_view res = stream.view();
    ASSERT_TRUE(res.find("User{id=101, name='Alice'}") != std::string_view::npos);
}

TEST_CASE("StreamBuffer - Header Format Without Method") {
    vordr::LogStream<1024> stream;
    vordr::ReflectedContext ctx{
        .method_name = "doAction",
        .class_name = "App::Controller",
        .full_scope_name = "App::Controller::doAction",
        .file_name = "Controller.cpp",
        .line_number = 77
    };

    stream.write_header(ctx, vordr::LogLevel::INFO, false);
    stream << "Starting transaction";

    std::string res(stream.view());
    // Pattern: date-time upto microsecond - LogLevel - Filename (without path) :Linenumber : Actual Log Entry
    // e.g. YYYY-MM-DD HH:MM:SS.ffffff - INFO - Controller.cpp:77 : Starting transaction
    std::regex log_regex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6} - INFO - Controller\.cpp:77 : Starting transaction$)");
    ASSERT_TRUE(std::regex_match(res, log_regex));
}

TEST_CASE("StreamBuffer - Header Format With Method") {
    vordr::LogStream<1024> stream;
    vordr::ReflectedContext ctx{
        .method_name = "doAction",
        .class_name = "App::Controller",
        .full_scope_name = "App::Controller::doAction",
        .file_name = "Controller.cpp",
        .line_number = 77
    };

    stream.write_header(ctx, vordr::LogLevel::DEBUG, true);
    stream << "Processing request";

    std::string res(stream.view());
    // Must include [App::Controller::doAction]
    std::regex log_regex(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{6} - DEBUG - Controller\.cpp:77 : \[App::Controller::doAction\] Processing request$)");
    ASSERT_TRUE(std::regex_match(res, log_regex));
}
