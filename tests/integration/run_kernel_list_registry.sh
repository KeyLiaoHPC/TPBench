#!/usr/bin/env bash
# Registry-driven kernel list/build: Tags column, N/A rows, multi-kernel build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${ROOT}/build"
TPBCLI="${BUILD}/bin/tpbcli"
WS="${BUILD}/tests/kernel_registry_ws"
TPB_HOME="${BUILD}"

rm -rf "${WS}"
mkdir -p "${WS}"

export TPB_HOME
export TPB_WORKSPACE="${WS}"

list_out="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel list 2>&1)"
echo "${list_out}"

echo "${list_out}" | grep -q "Tags"
echo "${list_out}" | grep -q "stream_mpi"
echo "${list_out}" | grep "stream_mpi" | grep -q "N/A"

TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel build --kernel 'staxpy,striad' --cflags "-O2"

list_after="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel list 2>&1)"
echo "${list_after}"

echo "${list_after}" | grep "staxpy" | grep -qv "N/A"
echo "${list_after}" | grep "striad" | grep -qv "N/A"

SHLIB_EXT=".so"
case "$(uname -s)" in
    Darwin) SHLIB_EXT=".dylib" ;;
esac
test -f "${TPB_HOME}/lib/libtpbk_staxpy${SHLIB_EXT}"
test -f "${TPB_HOME}/lib/libtpbk_striad${SHLIB_EXT}"

echo "kernel list/registry integration test passed"
