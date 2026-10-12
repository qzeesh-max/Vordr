#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Vordr Contributors
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

IMAGE_NAME="vordr:linux"
RUN_STRACE=0
RUN_SHELL=0
REBUILD_IMAGE=0

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -b, --build        Force rebuild Docker image before running"
    echo "  -s, --strace       Execute benchmarks inside Linux container under strace -c (measure kernel syscalls)"
    echo "  -i, --shell        Open an interactive bash shell inside the Linux container"
    echo "  -h, --help         Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build|--rebuild)
            REBUILD_IMAGE=1
            shift
            ;;
        -s|--strace)
            RUN_STRACE=1
            shift
            ;;
        -i|--shell)
            RUN_SHELL=1
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

# Verify Docker daemon is running
if ! docker info &>/dev/null; then
    echo -e "${RED}Error: Docker daemon is not accessible. Please ensure Docker or colima is running.${NC}"
    exit 1
fi

# Build Docker image if not present or rebuild requested
if [[ ${REBUILD_IMAGE} -eq 1 ]] || ! docker image inspect "${IMAGE_NAME}" &>/dev/null; then
    echo -e "${BLUE}=== Building Vordr Linux Docker Image (${IMAGE_NAME}) ===${NC}"
    docker build -t "${IMAGE_NAME}" -f "${ROOT_DIR}/Dockerfile" "${ROOT_DIR}"
fi

echo -e "${BLUE}=== Executing Vordr in Linux Docker Container ===${NC}"

if [[ ${RUN_SHELL} -eq 1 ]]; then
    docker run --rm -it "${IMAGE_NAME}" /bin/bash
elif [[ ${RUN_STRACE} -eq 1 ]]; then
    echo -e "${YELLOW}Running Linux strace -c syscall profiling on benchmarks...${NC}"
    docker run --rm "${IMAGE_NAME}" /bin/bash -c "./scripts/build.sh --release && strace -c ./build/vordr_benchmarks"
else
    docker run --rm "${IMAGE_NAME}" /bin/bash -c "./scripts/build.sh --release && ./run_tests.sh && ./run_benchmarks.sh"
fi

echo -e "${GREEN}Linux Docker execution completed successfully!${NC}"
