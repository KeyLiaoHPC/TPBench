#!/usr/bin/env bash
# Pack C7: NPB-MPI Class S/W smoke, illegal args, and official verify.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD:-${ROOT}/build}"
TPBCLI="${BUILD}/bin/tpbcli"
WS="${BUILD}/tests/npb_mpi_c7_ws"

if [[ ! -x "${TPBCLI}" ]]; then
    echo "SKIP: tpbcli not built"
    exit 0
fi
if [[ ! -f "${BUILD}/lib/libtpbk_ep_mpi.so" ]]; then
    echo "SKIP: NPB-MPI kernels not built"
    exit 0
fi
if ! command -v mpirun >/dev/null 2>&1; then
    echo "SKIP: mpirun not in PATH"
    exit 0
fi

rm -rf "${WS}"
mkdir -p "${WS}"
export TPB_HOME="${BUILD}"
export TPB_WORKSPACE="${WS}"
export LD_LIBRARY_PATH="${BUILD}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

help_txt="${BUILD}/tpb_cmake_help.txt"
if [[ -f "${help_txt}" ]]; then
    for k in bt_mpi sp_mpi lu_mpi cg_mpi mg_mpi ft_mpi ep_mpi is_mpi dt_mpi; do
        grep -q "${k}" "${help_txt}"
    done
fi

run_ok() {
    local kern="$1"
    local kargs="$2"
    local np="$3"
    local out
    out="$("${TPBCLI}" run --kernel "${kern}" --kargs "${kargs}" \
        --wrapper mpirun --wrapper-args "-np ${np}" 2>&1)" || {
        echo "${out}"
        echo "FAIL: ${kern} ${kargs} np=${np} exited nonzero"
        return 1
    }
    echo "${out}"
    echo "${out}" | grep -q "SUCCESSFUL"
    echo "${out}" | grep -q "Time in seconds"
    echo "${out}" | grep -q "Mop/s total"
}

run_fail_cause() {
    local kern="$1"
    local kargs="$2"
    local np="$3"
    set +e
    "${TPBCLI}" run --kernel "${kern}" --kargs "${kargs}" \
        --wrapper mpirun --wrapper-args "-np ${np}" >/tmp/tpb_npb_c7.out 2>&1
    local rc=$?
    set -e
    if [[ "${rc}" -eq 0 ]]; then
        cat /tmp/tpb_npb_c7.out
        echo "FAIL: expected nonzero for ${kern} ${kargs} np=${np}"
        return 1
    fi
}

# Same .so accepts Class S and W.
run_ok ep_mpi class=S 4
run_ok ep_mpi class=W 4
run_ok is_mpi class=S 4
run_ok cg_mpi class=S 4
run_ok bt_mpi class=S 4
run_ok sp_mpi class=S 4

# Illegal class / nprocs.
run_fail_cause ep_mpi class=Z 4
run_fail_cause bt_mpi class=S 3
run_fail_cause is_mpi class=S 3

echo "C7 NPB-MPI Class S integration test passed"
