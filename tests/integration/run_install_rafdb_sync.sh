#!/usr/bin/env bash
# Integration test: rafdb metadata sync on cmake --install (Pack C6).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${ROOT}/build"
PREFIX="${BUILD}/tests/install_rafdb_c6_ws"

if [[ ! -f "${BUILD}/rafdb/kernel/kernel.tpbe" ]] \
        && [[ ! -f "${BUILD}/rafdb/runtime_environment/runtime_environment.tpbe" ]]; then
    echo "C6: build tree missing rafdb metadata; run a full build with kernels first" >&2
    exit 1
fi

# C6.1 — empty TPB_HOME prefix: auto-sync kernel + runtime_environment metadata
rm -rf "${PREFIX}"
TPB_HOME="${PREFIX}" cmake --install "${BUILD}" --prefix "${PREFIX}" >/dev/null
test -f "${PREFIX}/rafdb/kernel/kernel.tpbe"
test -f "${PREFIX}/rafdb/runtime_environment/runtime_environment.tpbe"
"${PREFIX}/bin/tpbcli" --workspace "${PREFIX}" kernel get --kernel stream >/dev/null

# C6.2 — nonempty kernel domain, no TTY: skip kernel overwrite; dummy marker remains
touch "${PREFIX}/rafdb/kernel/dummy.tpbr"
rm -rf "${PREFIX}/rafdb/runtime_environment"
mkdir -p "${PREFIX}/rafdb/runtime_environment"
TPB_HOME="${PREFIX}" cmake --install "${BUILD}" --prefix "${PREFIX}" >/dev/null
test -f "${PREFIX}/rafdb/kernel/dummy.tpbr"
test -f "${PREFIX}/rafdb/runtime_environment/runtime_environment.tpbe"

# C6.3 — TPB_INSTALL_RAFDB=YES: backup and overwrite
TPB_INSTALL_RAFDB=YES TPB_HOME="${PREFIX}" cmake --install "${BUILD}" --prefix "${PREFIX}" >/dev/null
test -d "${PREFIX}/rafdb.bak.d"
test -n "$(ls -d "${PREFIX}/rafdb.bak.d"/rafdb_* 2>/dev/null | head -1)"
test ! -f "${PREFIX}/rafdb/kernel/dummy.tpbr"
test -f "${PREFIX}/rafdb/kernel/kernel.tpbe"

# C6.4 — unset TPB_HOME: default to $HOME/.tpbench with STATUS notice
MOCK_HOME="${BUILD}/tests/install_rafdb_c6_mock_home"
rm -rf "${MOCK_HOME}"
mkdir -p "${MOCK_HOME}"
LOG="${BUILD}/tests/install_rafdb_c6_default.log"
env -u TPB_HOME HOME="${MOCK_HOME}" \
    cmake --install "${BUILD}" --prefix "${MOCK_HOME}/.tpbench" >"${LOG}" 2>&1
grep -q "TPB_HOME not set" "${LOG}"
test -f "${MOCK_HOME}/.tpbench/rafdb/kernel/kernel.tpbe"

echo "install rafdb sync test passed"
