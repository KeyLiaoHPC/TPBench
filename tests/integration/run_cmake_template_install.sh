#!/usr/bin/env bash
# Verify functional CMake package and kernel templates are staged under build tree.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${ROOT}/build"

test -f "${BUILD}/lib/cmake/TPBench/TPBenchConfig.cmake"
test -f "${BUILD}/lib/cmake/TPBench/TPBenchKernel.cmake"
test -f "${BUILD}/etc/cmake/kernel/CMakeLists.txt.in"
test -f "${BUILD}/etc/cmake/kernel/tpbk_kernel.c.in"

echo "cmake package and template staging test passed"
