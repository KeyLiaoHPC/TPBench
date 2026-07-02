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

count_version_rows() {
    awk '/^Kernel Versions:/{found=1; next} found && /^[0-9a-f]{40} /{c++} END{print c+0}'
}

patch_template_sleep() {
    local source="$1"
    local seconds="$2"

    sed -i '/#include <stdint.h>/a #include <unistd.h>' "${source}"
    sed -i "/value\\[0\\] = 0\\.0;/i\\    sleep(${seconds});" "${source}"
}

rm -rf "${WS}" "${DIR_A}" "${DIR_B}"
mkdir -p "${WS}"

export TPB_HOME
export TPB_WORKSPACE="${WS}"

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel init --dir "${DIR_A}" --kernel "${KERNEL}"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel init --dir "${DIR_B}" --kernel "${KERNEL}"
patch_template_sleep "${DIR_A}/tpbk_${KERNEL}.c" 1
patch_template_sleep "${DIR_B}/tpbk_${KERNEL}.c" 2

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel build --dir "${DIR_A}" --kernel "${KERNEL}" \
    --cflags "-O2 -DTMPL_VARIANT=1"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" run --kernel "${KERNEL}" --kargs n=5

out_a="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel get -v --kernel "${KERNEL}" 2>&1)"
echo "${out_a}"
test "$(echo "${out_a}" | count_active_entries)" -eq 1
test "$(echo "${out_a}" | count_version_rows)" -eq 1

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel build --dir "${DIR_B}" --kernel "${KERNEL}" \
    --cflags "-O3 -DTMPL_VARIANT=2"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" run --kernel "${KERNEL}" --kargs n=6

out_b="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel get -v --kernel "${KERNEL}" 2>&1)"
echo "${out_b}"
test "$(echo "${out_b}" | count_active_entries)" -eq 1
test "$(echo "${out_b}" | count_version_rows)" -eq 2

TPB_HOME="${TPB_HOME}" "${TPBCLI}" kernel build --dir "${DIR_A}" --kernel "${KERNEL}" \
    --cflags "-O2 -DTMPL_VARIANT=1"
TPB_HOME="${TPB_HOME}" "${TPBCLI}" run --kernel "${KERNEL}" --kargs n=7

out_final="$(TPB_HOME="${TPB_HOME}" TPB_WORKSPACE="${WS}" \
    "${TPBCLI}" kernel get -v --kernel "${KERNEL}" 2>&1)"
echo "${out_final}"
test "$(echo "${out_final}" | count_active_entries)" -eq 1
test "$(echo "${out_final}" | count_version_rows)" -eq 2

test -d "${TPB_HOME}/lib/inactive"

echo "template kernel active-switch test passed"
