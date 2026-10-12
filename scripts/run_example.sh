#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Vordr Contributors
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
EXAMPLE_BIN="${BUILD_DIR}/vordr_example"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

REBUILD=0

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -b, --build        Rebuild example binary before running"
    echo "  -h, --help         Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build|--rebuild)
            REBUILD=1
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

if [[ ! -f "${EXAMPLE_BIN}" ]] || [[ ${REBUILD} -eq 1 ]]; then
    echo -e "${YELLOW}Building example binary first...${NC}"
    "${SCRIPT_DIR}/build.sh"
fi

echo -e "${BLUE}=== Executing Vordr Demo Application ===${NC}"
"${EXAMPLE_BIN}"
