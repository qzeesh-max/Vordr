# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Vordr Contributors

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies, cmake, ninja, strace, and gcc-16/g++-16 from toolchain test PPA
RUN apt-get update -qq && \
    apt-get install -y -qq \
        software-properties-common \
        cmake \
        ninja-build \
        strace \
        gdb \
        git \
        ca-certificates && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    apt-get update -qq && \
    apt-get install -y -qq \
        gcc-16 \
        g++-16 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Configure GCC 16 as default compiler
ENV CXX=/usr/bin/g++-16
ENV CC=/usr/bin/gcc-16

WORKDIR /workspace

# Copy project files into container
COPY . /workspace

# Default command: build in release mode, run test suite, and run benchmarks
CMD ["/bin/bash", "-c", "./scripts/build.sh --release && ./run_tests.sh && ./run_benchmarks.sh"]
