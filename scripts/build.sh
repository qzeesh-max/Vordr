#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Vordr Contributors
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_TYPE="Debug"
CLEAN_BUILD=0
VERBOSE=0

# Colors for terminal output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -r, --release      Build in Release mode with optimizations (default: Debug)"
    echo "  -c, --clean        Clean build directory before compiling"
    echo "  -v, --verbose      Enable verbose build output"
    echo "  -h, --help         Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
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

# Compiler resolution: prioritize /opt/homebrew/bin/g++-16
if [[ -z "${CXX:-}" ]]; then
    if [[ -x "/opt/homebrew/bin/g++-16" ]]; then
        CXX_COMPILER="/opt/homebrew/bin/g++-16"
    elif command -v g++-16 &>/dev/null; then
        CXX_COMPILER="$(command -v g++-16)"
    else
        CXX_COMPILER="$(command -v g++ || command -v clang++)"
    fi
else
    CXX_COMPILER="${CXX}"
fi

BUILD_DIR="${ROOT_DIR}/build"

if [[ ${CLEAN_BUILD} -eq 1 ]] && [[ -d "${BUILD_DIR}" ]]; then
    echo -e "${YELLOW}Cleaning build directory: ${BUILD_DIR}${NC}"
    rm -rf "${BUILD_DIR}"
fi

# Generator selection
GENERATOR="Unix Makefiles"
if command -v ninja &>/dev/null; then
    GENERATOR="Ninja"
fi

echo -e "${BLUE}=== Building Vordr ===${NC}"
echo -e " Compiler  : ${GREEN}${CXX_COMPILER}${NC}"
echo -e " Build Type: ${GREEN}${BUILD_TYPE}${NC}"
echo -e " Generator : ${GREEN}${GENERATOR}${NC}"
echo -e " Build Dir : ${GREEN}${BUILD_DIR}${NC}"
echo "----------------------------------------"

CMAKE_FLAGS=(
    "-B" "${BUILD_DIR}"
    "-S" "${ROOT_DIR}"
    "-G" "${GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
)

cmake "${CMAKE_FLAGS[@]}"

if [[ ${VERBOSE} -eq 1 ]]; then
    cmake --build "${BUILD_DIR}" -v
else
    cmake --build "${BUILD_DIR}"
fi

echo -e "${GREEN}Build completed successfully!${NC}"
