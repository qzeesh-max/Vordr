// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"
#include "vordr/reflection.hpp"

namespace TestSpace {

class MyClass {
public:
    static consteval vordr::ReflectedContext get_context_static() {
        constexpr auto ctx = std::meta::access_context::current();
        return vordr::detail::get_context<ctx.scope()>("/some/deep/path/MyClass.cpp", 42);
    }

    vordr::ReflectedContext get_context_member() {
        constexpr auto ctx = std::meta::access_context::current();
        return vordr::detail::get_context<ctx.scope()>("/other/path/MyClass.hpp", 101);
    }

    vordr::ReflectedContext get_context_lambda() {
        auto l = []() {
            constexpr auto ctx = std::meta::access_context::current();
            return vordr::detail::get_context<ctx.scope()>("/nested/lambda/MyClass.cpp", 202);
        };
        return l();
    }
};

template <typename T>
class TemplateClass {
public:
    vordr::ReflectedContext get_context_tmpl() {
        constexpr auto ctx = std::meta::access_context::current();
        return vordr::detail::get_context<ctx.scope()>("/tmpl/path/Tmpl.hpp", 303);
    }
};

} // namespace TestSpace

static consteval vordr::ReflectedContext get_context_free() {
    constexpr auto ctx = std::meta::access_context::current();
    return vordr::detail::get_context<ctx.scope()>("/root/free_func.cpp", 999);
}

TEST_CASE("Reflection - Member Function") {
    TestSpace::MyClass obj;
    auto ctx = obj.get_context_member();
    ASSERT_EQ(ctx.class_name, "TestSpace::MyClass");
    ASSERT_EQ(ctx.method_name, "get_context_member");
    ASSERT_EQ(ctx.file_name, "MyClass.hpp");
    ASSERT_EQ(ctx.line_number, 101u);
}

TEST_CASE("Reflection - Static Member Function Consteval") {
    constexpr auto ctx = TestSpace::MyClass::get_context_static();
    static_assert(ctx.class_name == "TestSpace::MyClass");
    static_assert(ctx.method_name == "get_context_static");
    static_assert(ctx.file_name == "MyClass.cpp");
    static_assert(ctx.line_number == 42u);
    ASSERT_EQ(ctx.class_name, "TestSpace::MyClass");
}

TEST_CASE("Reflection - Free Function") {
    constexpr auto ctx = get_context_free();
    ASSERT_EQ(ctx.class_name, "");
    ASSERT_EQ(ctx.method_name, "get_context_free");
    ASSERT_EQ(ctx.file_name, "free_func.cpp");
    ASSERT_EQ(ctx.line_number, 999u);
}

TEST_CASE("Reflection - Lambda Inside Member Function") {
    TestSpace::MyClass obj;
    auto ctx = obj.get_context_lambda();
    ASSERT_EQ(ctx.class_name, "TestSpace::MyClass");
    ASSERT_EQ(ctx.file_name, "MyClass.cpp");
    ASSERT_EQ(ctx.line_number, 202u);
}

TEST_CASE("Reflection - Class Template") {
    TestSpace::TemplateClass<int> obj;
    auto ctx = obj.get_context_tmpl();
    ASSERT_EQ(ctx.method_name, "get_context_tmpl");
    ASSERT_EQ(ctx.file_name, "Tmpl.hpp");
    ASSERT_EQ(ctx.line_number, 303u);
    // Class name should contain TestSpace::TemplateClass<int>
    ASSERT_TRUE(ctx.class_name.find("TemplateClass") != std::string_view::npos);
}

TEST_CASE("Reflection - Path Stripping") {
    constexpr auto f1 = vordr::strip_file_path("/a/b/c/foo.cpp");
    ASSERT_EQ(f1, "foo.cpp");

    constexpr auto f2 = vordr::strip_file_path("bar.hpp");
    ASSERT_EQ(f2, "bar.hpp");

    constexpr auto f3 = vordr::strip_file_path("C:\\windows\\path\\baz.cc");
    ASSERT_EQ(f3, "baz.cc");

    constexpr auto f4 = vordr::strip_file_path("");
    ASSERT_EQ(f4, "");
}
