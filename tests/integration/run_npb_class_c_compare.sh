#!/usr/bin/env bash
# Host campaign: compare TPBench NPB-MPI FOMs with unmodified NASA binaries.
# Class C at 256 ranks for BT/SP/LU/CG/MG/FT/EP/IS; DT Class B only.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD:-${ROOT}/build}"
TPBCLI="${TPBCLI:-${BUILD}/bin/tpbcli}"
NASA_BIN="${NPB_NASA_BIN:-${ROOT}/../NPB3.4.4/NPB3.4-MPI/bin}"
export TPB_HOME="${TPB_HOME:-${BUILD}}"
export TPB_WORKSPACE="${TPB_WORKSPACE:-${BUILD}/tests/npb_class_c_ws}"
export LD_LIBRARY_PATH="${BUILD}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
mkdir -p "${TPB_WORKSPACE}"
NP="${NPB_COMPARE_NP:-256}"
REPEATS="${NPB_COMPARE_REPEATS:-3}"
BAND="${NPB_COMPARE_BAND:-0.05}"
CLS="${NPB_COMPARE_CLASS:-C}"
OUT="${NPB_COMPARE_OUT:-${BUILD}/tests/npb_class_${CLS,,}_compare.txt}"

usage() {
    cat <<EOF
Usage: $0
  Compares official NASA NPB-MPI stdout against tpbcli run for:
    BT SP LU CG MG FT EP IS  class ${CLS} at ${NP} ranks (3 repeats, median)
    DT class B at 43 ranks (BH) only when class is C (Class D DT has no verify table)
Environment:
  NPB_NASA_BIN        directory of NASA *.\${CLS}.x binaries
  TPBCLI              path to tpbcli (default ${TPBCLI})
  NPB_COMPARE_CLASS   problem class (default C)
  NPB_COMPARE_NP      rank count (default 256)
  NPB_COMPARE_BAND    relative FOM band (default 0.05)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ ! -x "${TPBCLI}" ]]; then
    echo "tpbcli not found: ${TPBCLI}"
    exit 1
fi

parse_nasa() {
    local f="$1"
    python3 - "$f" <<'PY'
import re, sys
text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
ver = 1 if re.search(r"Verification\s*=\s*SUCCESSFUL", text) else 0
def grab(pat):
    m = re.search(pat, text)
    return m.group(1) if m else ""
cls = grab(r"Class\s*=\s*(\S+)")
niter = grab(r"Iterations\s*=\s*([0-9]+)")
time = grab(r"Time in seconds\s*=\s*([0-9.]+)")
mops = grab(r"Mop/s total\s*=\s*([0-9.]+)")
if not mops:
    m = re.search(r"([0-9.]+)\s+bytes transmitted", text)
    mops = m.group(1) if m else ""
print(f"{ver}|{cls}|{niter}|{time}|{mops}")
PY
}

median3() {
    python3 -c "print(sorted([float('$1'), float('$2'), float('$3')])[1])"
}

rel_ok() {
    python3 -c "
a=float('$1'); b=float('$2'); band=float('$3')
print(0 if a==0 and b==0 else 0 if a>0 and abs(a-b)/a<=band else 1)
"
}

run_one_nasa() {
    local bin="$1"
    local extra="${2:-}"
    mpirun -np "${NP}" "${bin}" ${extra}
}

run_one_tpb() {
    local kern="$1"
    local kargs="$2"
    "${TPBCLI}" run --kernel "${kern}" --kargs "${kargs}" \
        --wrapper mpirun --wrapper-args "-np ${NP}"
}

declare -a ROWS
fail=0

compare_kernel() {
    local name="$1"
    local nasa_bin="$2"
    local kern="$3"
    local kargs="$4"
    local extra="${5:-}"
    local i nasa_out tpb_out
    local n_ver n_cls n_it n_t n_m
    local t_ver t_cls t_it t_t t_m
    local nt=() nm=() tt=() tm=()

    echo "== ${name} =="
    for i in 1 2 3; do
        nasa_out="$(run_one_nasa "${nasa_bin}" "${extra}" 2>&1 | tee "/tmp/npb_nasa_${name}_$i.txt")"
        tpb_out="$(run_one_tpb "${kern}" "${kargs}" 2>&1 | tee "/tmp/npb_tpb_${name}_$i.txt")"
        IFS='|' read -r n_ver n_cls n_it n_t n_m <<<"$(parse_nasa "/tmp/npb_nasa_${name}_$i.txt")"
        IFS='|' read -r t_ver t_cls t_it t_t t_m <<<"$(parse_nasa "/tmp/npb_tpb_${name}_$i.txt")"
        echo "  NASA[$i] ver=${n_ver} class=${n_cls} niter=${n_it} time=${n_t} mops=${n_m}"
        echo "  TPB [$i] ver=${t_ver} class=${t_cls} niter=${t_it} time=${t_t} mops=${t_m}"
        if [[ "${n_ver}" != "1" || "${t_ver}" != "1" ]]; then
            echo "  FAIL verification"
            fail=1
        fi
        nt+=("${n_t}"); nm+=("${n_m}"); tt+=("${t_t}"); tm+=("${t_m}")
    done
    local ntm ttm nmm tmm
    ntm="$(median3 "${nt[0]}" "${nt[1]}" "${nt[2]}")"
    ttm="$(median3 "${tt[0]}" "${tt[1]}" "${tt[2]}")"
    nmm="$(median3 "${nm[0]}" "${nm[1]}" "${nm[2]}")"
    tmm="$(median3 "${tm[0]}" "${tm[1]}" "${tm[2]}")"
    echo "  median Time NASA=${ntm} TPB=${ttm}"
    echo "  median Mop/s NASA=${nmm} TPB=${tmm}"
    if [[ "$(rel_ok "${ntm}" "${ttm}" "${BAND}")" != "0" ]]; then
        echo "  FAIL Time drift > ${BAND}"
        fail=1
    fi
    if [[ "$(rel_ok "${nmm}" "${tmm}" "${BAND}")" != "0" ]]; then
        echo "  FAIL Mop/s drift > ${BAND}"
        fail=1
    fi
}

mkdir -p "$(dirname "${OUT}")"
{
    echo "NPB Class ${CLS} compare  np=${NP}  band=${BAND}"
    compare_kernel BT "${NASA_BIN}/bt.${CLS}.x" bt_mpi "class=${CLS}"
    compare_kernel SP "${NASA_BIN}/sp.${CLS}.x" sp_mpi "class=${CLS}"
    compare_kernel LU "${NASA_BIN}/lu.${CLS}.x" lu_mpi "class=${CLS}"
    compare_kernel CG "${NASA_BIN}/cg.${CLS}.x" cg_mpi "class=${CLS}"
    compare_kernel MG "${NASA_BIN}/mg.${CLS}.x" mg_mpi "class=${CLS}"
    compare_kernel FT "${NASA_BIN}/ft.${CLS}.x" ft_mpi "class=${CLS}"
    compare_kernel EP "${NASA_BIN}/ep.${CLS}.x" ep_mpi "class=${CLS}"
    compare_kernel IS "${NASA_BIN}/is.${CLS}.x" is_mpi "class=${CLS}"
    if [[ "${CLS}" == "C" ]]; then
        NP_SAVE="${NP}"
        NP=43
        compare_kernel DT "${NASA_BIN}/dt.B.x" dt_mpi "class=B,graph=B" BH
        NP="${NP_SAVE}"
    fi
    if [[ "${fail}" -eq 0 ]]; then
        echo "ALL PASS"
    else
        echo "SOME FAIL"
    fi
} | tee "${OUT}"

exit "${fail}"
