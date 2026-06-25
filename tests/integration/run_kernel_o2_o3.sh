#!/usr/bin/env bash
# Build stream kernel with O2 and O3 into one workspace; verify metadata variants.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${ROOT}/build"
WS="${BUILD}/tests/kernel_o2o3_ws"
TPBCLI="${BUILD}/bin/tpbcli"

build_variant() {
    local flags="$1"
    local bdir="${ROOT}/build/o2o3_${flags}"
    cmake -S "${ROOT}" -B "${bdir}" \
        -DTPB_KERNELS=stream \
        -DTPB_KERNEL_CFLAGS="${flags}" \
        -DTPB_RECORD_KERNEL_COMPILE_HISTORY=ON \
        >/dev/null
    TPB_HOME="${bdir}" TPB_WORKSPACE="${WS}" cmake --build "${bdir}" --target tpbk_stream --clean-first >/dev/null
}

rm -rf "${WS}"
mkdir -p "${WS}"

build_variant "-O2"
build_variant "-O3"

export TPB_HOME="${BUILD}"
export TPB_WORKSPACE="${WS}"
out="$(TPB_WORKSPACE="${WS}" "${TPBCLI}" kernel get -v --kernel stream 2>&1)"

echo "${out}"
echo "${out}" | grep -q 'kernel_cflags=-O2'
echo "${out}" | grep -q 'kernel_cflags=-O3'
echo "O2/O3 kernel history test passed"
