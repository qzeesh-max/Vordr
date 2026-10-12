#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Vordr Contributors
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TEST_BIN="${BUILD_DIR}/vordr_tests"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

REBUILD=0
USE_CTEST=0

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -b, --build        Rebuild test binaries before running"
    echo "  --ctest            Run via CTest runner instead of direct executable"
    echo "  -h, --help         Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build|--rebuild)
            REBUILD=1
            shift
            ;;
        --ctest)
            USE_CTEST=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

if [[ ! -f "${TEST_BIN}" ]] || [[ ${REBUILD} -eq 1 ]]; then
    echo -e "${YELLOW}Building test binaries first...${NC}"
    "${SCRIPT_DIR}/build.sh"
fi

echo -e "${BLUE}=== Running Vordr Unit & Correctness Tests ===${NC}"

if [[ ${USE_CTEST} -eq 1 ]]; then
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
else
    "${TEST_BIN}"
fi

echo -e "${GREEN}All tests passed!${NC}"
