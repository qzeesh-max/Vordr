// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Vordr Contributors

#include "test_framework.hpp"

int main() {
    return ::vordr::testing::TestRegistry::instance().run_all();
}
