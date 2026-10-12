// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/config.hpp"
#include "vordr/xml_parser.hpp"

TEST_CASE("Config - Default Central Level") {
    vordr::Config cfg;
    ASSERT_EQ(cfg.central_level(), vordr::LogLevel::INFO);
    ASSERT_FALSE(cfg.emit_method());

    // In default INFO:
    // TRACE & DEBUG should be disabled
    // INFO, WARN, ERROR, FATAL should be enabled
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::TRACE, "main.cpp", "MyClass", "myMethod"));
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::DEBUG, "main.cpp", "MyClass", "myMethod"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::INFO, "main.cpp", "MyClass", "myMethod"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::WARN, "main.cpp", "MyClass", "myMethod"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::ERROR, "main.cpp", "MyClass", "myMethod"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::FATAL, "main.cpp", "MyClass", "myMethod"));
}

TEST_CASE("Config - File Level Override") {
    vordr::Config cfg;
    cfg.set_central_level(vordr::LogLevel::WARN);

    // Override specifically for service.cpp
    cfg.set_file_level("service.cpp", vordr::LogLevel::DEBUG);

    // service.cpp gets DEBUG
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::DEBUG, "service.cpp", "Worker", "run"));
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::TRACE, "service.cpp", "Worker", "run"));

    // other files fall back to central WARN
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::DEBUG, "other.cpp", "Worker", "run"));
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::INFO, "other.cpp", "Worker", "run"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::WARN, "other.cpp", "Worker", "run"));

    // Path stripping in file match
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::DEBUG, "/opt/src/service.cpp", "Worker", "run"));
}

TEST_CASE("Config - Class Level Override") {
    vordr::Config cfg;
    cfg.set_central_level(vordr::LogLevel::ERROR);
    cfg.set_file_level("service.cpp", vordr::LogLevel::WARN);

    // Override for Net::Client
    cfg.set_class_level("Net::Client", vordr::LogLevel::TRACE);

    // Net::Client in service.cpp should get TRACE (overrides file and central)
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::TRACE, "service.cpp", "Net::Client", "connect"));

    // Another class in service.cpp should get file level (WARN)
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::INFO, "service.cpp", "Db::Client", "connect"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::WARN, "service.cpp", "Db::Client", "connect"));

    // Unqualified class lookup
    cfg.set_class_level("SpecialWorker", vordr::LogLevel::DEBUG);
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::DEBUG, "foo.cpp", "App::SpecialWorker", "work"));
}

TEST_CASE("Config - Method Level Override") {
    vordr::Config cfg;
    cfg.set_central_level(vordr::LogLevel::ERROR);
    cfg.set_file_level("db.cpp", vordr::LogLevel::WARN);
    cfg.set_class_level("Db::Session", vordr::LogLevel::INFO);

    // Qualified method override
    cfg.set_method_level("Db::Session::execute", vordr::LogLevel::TRACE);

    // execute gets TRACE (overriding class INFO, file WARN, central ERROR)
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::TRACE, "db.cpp", "Db::Session", "execute"));

    // another method in Db::Session gets class level (INFO)
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::DEBUG, "db.cpp", "Db::Session", "commit"));
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::INFO, "db.cpp", "Db::Session", "commit"));

    // Bare method override
    cfg.set_method_level("criticalTask", vordr::LogLevel::TRACE);
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::TRACE, "other.cpp", "RandomClass", "criticalTask"));
}

TEST_CASE("Config - OFF Level") {
    vordr::Config cfg;
    cfg.set_central_level(vordr::LogLevel::OFF);
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::FATAL, "a.cpp", "B", "c"));

    cfg.set_central_level(vordr::LogLevel::TRACE);
    cfg.set_file_level("silent.cpp", vordr::LogLevel::OFF);
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::FATAL, "silent.cpp", "B", "c"));
}

TEST_CASE("XML Config - Full Document Parsing") {
    constexpr std::string_view xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!-- Sample Vordr configuration -->
<vordr>
    <sink path="custom_app.log" max_size="1048576" />
    <logging level="WARN" emit_method="true">
        <file name="network.cpp" level="DEBUG" />
        <class name="App::DbService" level="INFO" />
        <method name="App::DbService::executeRaw" level="TRACE" />
    </logging>
</vordr>
)";

    vordr::Config cfg;
    std::string err;
    bool ok = vordr::XmlConfigParser::parse_string(xml, cfg, &err);
    ASSERT_TRUE(ok);
    ASSERT_EQ(cfg.sink_path(), "custom_app.log");
    ASSERT_EQ(cfg.sink_max_size(), 1048576u);
    ASSERT_EQ(cfg.central_level(), vordr::LogLevel::WARN);
    ASSERT_TRUE(cfg.emit_method());

    // File level
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::DEBUG, "network.cpp", "Client", "recv"));
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::DEBUG, "other.cpp", "Client", "recv"));

    // Class level
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::INFO, "db.cpp", "App::DbService", "connect"));
    ASSERT_FALSE(cfg.is_enabled(vordr::LogLevel::DEBUG, "db.cpp", "App::DbService", "connect"));

    // Method level
    ASSERT_TRUE(cfg.is_enabled(vordr::LogLevel::TRACE, "db.cpp", "App::DbService", "executeRaw"));
}

TEST_CASE("XML Config - Malformed Syntax Handling") {
    constexpr std::string_view bad_xml = "<vordr><sink path=\"log\"></bad_tag></vordr>";
    vordr::Config cfg;
    std::string err;
    bool ok = vordr::XmlConfigParser::parse_string(bad_xml, cfg, &err);
    ASSERT_FALSE(ok);
    ASSERT_FALSE(err.empty());
}

TEST_CASE("Config - Thread ID Settings and XML Attributes") {
    // 1. Default should be true
    vordr::Config cfg;
    ASSERT_TRUE(cfg.emit_thread_id());

    // 2. Manual toggle
    cfg.set_emit_thread_id(false);
    ASSERT_FALSE(cfg.emit_thread_id());
    cfg.set_emit_thread_id(true);
    ASSERT_TRUE(cfg.emit_thread_id());

    // 3. XML parsing with emit_thread_id="false"
    constexpr std::string_view xml_disabled = R"(<vordr emit_thread_id="false"><sink path="test.log" /></vordr>)";
    std::string err;
    ASSERT_TRUE(vordr::XmlConfigParser::parse_string(xml_disabled, cfg, &err));
    ASSERT_FALSE(cfg.emit_thread_id());

    // 4. XML parsing with emit_thread_id="true"
    constexpr std::string_view xml_enabled = R"(<vordr emit_thread_id="true"><sink path="test.log" /></vordr>)";
    ASSERT_TRUE(vordr::XmlConfigParser::parse_string(xml_enabled, cfg, &err));
    ASSERT_TRUE(cfg.emit_thread_id());

    // 5. XML parsing on logging node
    constexpr std::string_view xml_logging = R"(<vordr><logging level="INFO" emit_thread_id="false" /></vordr>)";
    ASSERT_TRUE(vordr::XmlConfigParser::parse_string(xml_logging, cfg, &err));
    ASSERT_FALSE(cfg.emit_thread_id());
}

TEST_CASE("Config - Sink Append Setting and XML Attribute") {
    // 1. Default should be true
    vordr::Config cfg;
    ASSERT_TRUE(cfg.sink_append());

    // 2. Manual toggle
    cfg.set_sink_append(false);
    ASSERT_FALSE(cfg.sink_append());
    cfg.set_sink_append(true);
    ASSERT_TRUE(cfg.sink_append());

    // 3. XML parsing with append="false" on <sink>
    constexpr std::string_view xml_no_append = R"(<vordr><sink path="rotated.log" append="false" /></vordr>)";
    std::string err;
    ASSERT_TRUE(vordr::XmlConfigParser::parse_string(xml_no_append, cfg, &err));
    ASSERT_FALSE(cfg.sink_append());
    ASSERT_EQ(cfg.sink_path(), "rotated.log");

    // 4. XML parsing with append="true" on <sink>
    constexpr std::string_view xml_append = R"(<vordr><sink path="appended.log" append="true" /></vordr>)";
    ASSERT_TRUE(vordr::XmlConfigParser::parse_string(xml_append, cfg, &err));
    ASSERT_TRUE(cfg.sink_append());
    ASSERT_EQ(cfg.sink_path(), "appended.log");
}


