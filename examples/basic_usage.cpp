// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "vordr/vordr.hpp"
#include <iostream>

namespace Demo {

class DatabaseManager {
public:
    void connect() {
        VORDR_INFO << "Connecting to database cluster...";
    }

    void executeQuery(const std::string& query) {
        VORDR_DEBUG << "Executing SQL query: " << query;
        VORDR_TRACE << "Query execution plan generated";
    }
};

class OrderProcessor {
public:
    void process(int order_id, double amount) {
        VORDR_INFO << "Processing order #" << order_id << " for amount $" << amount;
    }
};

} // namespace Demo

int expensiveComputation() {
    std::cout << "[CALLED] expensiveComputation()\n";
    return 1337;
}

int main() {
    std::cout << "=== Vordr Logger Framework Demo (C++26 Reflection) ===\n";

    constexpr std::string_view xml_config = R"(<?xml version="1.0" encoding="UTF-8"?>
<vordr>
    <sink path="/tmp/vordr_demo.log" max_size="10485760" />
    <logging level="INFO" emit_method="true">
        <class name="Demo::DatabaseManager" level="TRACE" />
    </logging>
</vordr>
)";

    auto& logger = vordr::Logger::instance();
    std::string err;
    if (!logger.init_xml_string(xml_config, &err)) {
        std::cerr << "Failed to init config: " << err << "\n";
        return 1;
    }

    Demo::DatabaseManager db;
    db.connect();
    db.executeQuery("SELECT * FROM users WHERE active = 1");

    Demo::OrderProcessor orders;
    orders.process(4040, 249.99);

    // Demonstration of lazy evaluation:
    // OrderProcessor is at central level (INFO). TRACE is disabled!
    // expensiveComputation() will NEVER be executed!
    VORDR_TRACE << "Expensive debug value: " << expensiveComputation();

    logger.flush(true);

    std::cout << "\n--- Generated Memory-Mapped Log Output ---\n";
    std::cout << logger.sink()->written_content();
    std::cout << "--- End of Log ---\n";

    return 0;
}
