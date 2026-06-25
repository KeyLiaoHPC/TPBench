#!/usr/bin/env bash
# Build the same template kernel name from two directories; verify active switching.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${ROOT}/build"
TPBCLI="${BUILD}/bin/tpbcli"
WS="${BUILD}/tests/kernel_tmplswitch_ws"
TPB_HOME="${BUILD}"
DIR_A="${BUILD}/tests/kinit_a"
DIR_B="${BUILD}/tests/kinit_b"
KERNEL="tmplswitch"

count_active_entries() {
    grep -c '^Active: 1$' || true
}

rm -rf "${WS}" "${DIR_A}" "${DIR_B}"
mkdir -p "${WS}"

export TPB_HOME
export TPB_WORKSPACE="${WS}"

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel init --dir "${DIR_A}" --kernel "${KERNEL}"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel init --dir "${DIR_B}" --kernel "${KERNEL}"

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel build --dir "${DIR_A}" --kernel "${KERNEL}" \
    --cflags "-O2 -DTMPL_VARIANT=1"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" run --kernel "${KERNEL}" --kargs n=5

out_a="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel get -v --kernel "${KERNEL}" 2>&1)"
echo "${out_a}"
test "$(echo "${out_a}" | count_active_entries)" -eq 1
echo "${out_a}" | grep -q 'kernel_cflags=-O2'
echo "${out_a}" | grep -q "${DIR_A}"

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel build --dir "${DIR_B}" --kernel "${KERNEL}" \
    --cflags "-O3 -DTMPL_VARIANT=2"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" run --kernel "${KERNEL}" --kargs n=6

out_b="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel get -v --kernel "${KERNEL}" 2>&1)"
echo "${out_b}"
test "$(echo "${out_b}" | count_active_entries)" -eq 1
echo "${out_b}" | grep -q 'kernel_cflags=-O2'
echo "${out_b}" | grep -q 'kernel_cflags=-O3'
echo "${out_b}" | grep -q "${DIR_B}"

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel build --dir "${DIR_A}" --kernel "${KERNEL}" \
    --cflags "-O2 -DTMPL_VARIANT=1"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" run --kernel "${KERNEL}" --kargs n=7

out_final="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel get -v --kernel "${KERNEL}" 2>&1)"
echo "${out_final}"
test "$(echo "${out_final}" | count_active_entries)" -eq 1
echo "${out_final}" | grep -q 'kernel_cflags=-O2'
echo "${out_final}" | grep -q 'kernel_cflags=-O3'
echo "${out_final}" | grep -q "${DIR_A}"
echo "${out_final}" | grep -q "${DIR_B}"

test -d "${TPB_HOME}/lib/inactive"

echo "template kernel active-switch test passed"
