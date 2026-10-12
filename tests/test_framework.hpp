// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <sstream>
#include <source_location>

namespace vordr::testing {

struct TestFailure {
    std::string message;
    std::string file;
    int line;
};

class TestCase {
public:
    using TestFunc = std::function<void(TestCase&)>;

    TestCase(std::string name, TestFunc func)
        : name_(std::move(name)), func_(std::move(func)) {}

    const std::string& name() const { return name_; }

    bool run() {
        failures_.clear();
        auto start = std::chrono::steady_clock::now();
        try {
            func_(*this);
        } catch (const std::exception& e) {
            fail(std::string("Unhandled exception: ") + e.what(), __FILE__, __LINE__);
        } catch (...) {
            fail("Unhandled non-std exception", __FILE__, __LINE__);
        }
        auto end = std::chrono::steady_clock::now();
        duration_ms_ = std::chrono::duration<double, std::milli>(end - start).count();
        return failures_.empty();
    }

    void fail(std::string message, std::string file = "", int line = 0) {
        failures_.push_back({std::move(message), std::move(file), line});
    }

    const std::vector<TestFailure>& failures() const { return failures_; }
    double duration_ms() const { return duration_ms_; }

private:
    std::string name_;
    TestFunc func_;
    std::vector<TestFailure> failures_;
    double duration_ms_{0.0};
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void register_test(std::string name, TestCase::TestFunc func) {
        tests_.emplace_back(std::move(name), std::move(func));
    }

    int run_all() {
        std::cout << "==================================================\n";
        std::cout << " Running " << tests_.size() << " test case(s)...\n";
        std::cout << "==================================================\n";

        size_t passed = 0;
        size_t failed = 0;

        for (auto& test : tests_) {
            std::cout << "[ RUN      ] " << test.name() << "\n";
            if (test.run()) {
                std::cout << "[       OK ] " << test.name() << " (" << test.duration_ms() << " ms)\n";
                ++passed;
            } else {
                std::cout << "[  FAILED  ] " << test.name() << " (" << test.duration_ms() << " ms)\n";
                for (const auto& f : test.failures()) {
                    std::cout << "    " << f.file << ":" << f.line << ": " << f.message << "\n";
                }
                ++failed;
            }
        }

        std::cout << "==================================================\n";
        std::cout << " Summary: " << passed << " passed, " << failed << " failed, "
                  << tests_.size() << " total.\n";
        std::cout << "==================================================\n";

        return failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct AutoRegister {
    AutoRegister(std::string name, TestCase::TestFunc func) {
        TestRegistry::instance().register_test(std::move(name), std::move(func));
    }
};

#define VORDR_CONCAT_IMPL(x, y) x##y
#define VORDR_CONCAT(x, y) VORDR_CONCAT_IMPL(x, y)

#define TEST_CASE(name) \
    static void VORDR_CONCAT(_vordr_test_, __LINE__)(::vordr::testing::TestCase& _tc); \
    static ::vordr::testing::AutoRegister VORDR_CONCAT(_vordr_reg_, __LINE__)(name, VORDR_CONCAT(_vordr_test_, __LINE__)); \
    static void VORDR_CONCAT(_vordr_test_, __LINE__)(::vordr::testing::TestCase& _tc)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            _tc.fail(std::string("Assertion failed: ") + #cond, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(cond) \
    do { \
        if (cond) { \
            _tc.fail(std::string("Assertion failed (expected false): ") + #cond, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va == _vb)) { \
            std::ostringstream _oss; \
            _oss << "Assertion failed: " << #a << " == " << #b << " (got " << _va << " vs " << _vb << ")"; \
            _tc.fail(_oss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        auto _va = (a); \
        auto _vb = (b); \
        if (!(_va != _vb)) { \
            std::ostringstream _oss; \
            _oss << "Assertion failed: " << #a << " != " << #b << " (both equal " << _va << ")"; \
            _tc.fail(_oss.str(), __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

} // namespace vordr::testing
