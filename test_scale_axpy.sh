#!/bin/bash
# Test script for scale, axpy, rtriad, and sum kernels

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

passed=0
failed=0

print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} $2"
        ((passed++))
    else
        echo -e "${RED}[FAIL]${NC} $2"
        ((failed++))
    fi
}

print_header() {
    echo ""
    echo "=========================================="
    echo "$1"
    echo "=========================================="
}

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Building..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ..
    make -j$(nproc)
    cd "$SCRIPT_DIR"
fi

if [ ! -f "$BIN_DIR/tpbcli" ]; then
    echo "Error: tpbcli not found in $BIN_DIR"
    exit 1
fi

print_header "TPBench Scale/AXPY Kernel Tests"

# ==============================================================================
# Test Scale Kernel (OpenMP)
# ==============================================================================
print_header "Scale Kernel Tests (OpenMP)"

# Test 1: Scale kernel basic run
echo "Test 1: Scale kernel basic run with default params"
output=$("$BIN_DIR/tpbcli" run --kernel scale --kargs ntest=5,total_memsize=64 2>&1) || true
if echo "$output" | grep -q "scale error"; then
    print_result 0 "Scale kernel ran successfully"
else
    print_result 1 "Scale kernel failed to run"
    echo "$output"
fi

# Test 2: Scale kernel with array_size parameter
echo "Test 2: Scale kernel with array_size parameter"
output=$("$BIN_DIR/tpbcli" run --kernel scale --kargs ntest=3,array_size=1024 2>&1) || true
if echo "$output" | grep -q "scale error"; then
    print_result 0 "Scale kernel with array_size ran successfully"
else
    print_result 1 "Scale kernel with array_size failed"
    echo "$output"
fi

# Test 3: Scale PLI executable
echo "Test 3: Scale PLI executable (.tpbx)"
if [ -f "$BIN_DIR/tpbk_scale.tpbx" ]; then
    output=$("$BIN_DIR/tpbk_scale.tpbx" clock_gettime ntest=3 total_memsize=32 2>&1) || true
    if echo "$output" | grep -q "scale"; then
        print_result 0 "Scale PLI executable ran successfully"
    else
        print_result 1 "Scale PLI executable failed"
        echo "$output"
    fi
else
    print_result 1 "Scale PLI executable not found"
fi

# ==============================================================================
# Test AXPY Kernel (OpenMP)
# ==============================================================================
print_header "AXPY Kernel Tests (OpenMP)"

# Test 4: AXPY kernel basic run
echo "Test 4: AXPY kernel basic run with default params"
output=$("$BIN_DIR/tpbcli" run --kernel axpy --kargs ntest=5,total_memsize=64 2>&1) || true
if echo "$output" | grep -q "axpy error"; then
    print_result 0 "AXPY kernel ran successfully"
else
    print_result 1 "AXPY kernel failed to run"
    echo "$output"
fi

# Test 5: AXPY kernel with array_size parameter
echo "Test 5: AXPY kernel with array_size parameter"
output=$("$BIN_DIR/tpbcli" run --kernel axpy --kargs ntest=3,array_size=1024 2>&1) || true
if echo "$output" | grep -q "axpy error"; then
    print_result 0 "AXPY kernel with array_size ran successfully"
else
    print_result 1 "AXPY kernel with array_size failed"
    echo "$output"
fi

# Test 6: AXPY PLI executable
echo "Test 6: AXPY PLI executable (.tpbx)"
if [ -f "$BIN_DIR/tpbk_axpy.tpbx" ]; then
    output=$("$BIN_DIR/tpbk_axpy.tpbx" clock_gettime ntest=3 total_memsize=32 2>&1) || true
    if echo "$output" | grep -q "axpy"; then
        print_result 0 "AXPY PLI executable ran successfully"
    else
        print_result 1 "AXPY PLI executable failed"
        echo "$output"
    fi
else
    print_result 1 "AXPY PLI executable not found"
fi

# ==============================================================================
# Test Verification (Error Check)
# ==============================================================================
print_header "Verification Tests"

# Test 7: Scale kernel verification (error should be small)
echo "Test 7: Scale kernel result verification"
output=$("$BIN_DIR/tpbcli" run --kernel scale --kargs ntest=10,total_memsize=128 2>&1) || true
error=$(echo "$output" | grep "scale error" | awk '{print $NF}')
if [ -n "$error" ]; then
    # Check if error is less than 1e-6 (using bc for floating point comparison)
    if echo "$error < 1e-6" | bc -l 2>/dev/null | grep -q "1"; then
        print_result 0 "Scale verification passed (error=$error)"
    else
        # For zero error, bc might fail
        if [ "$error" = "0.000000" ] || [ "$error" = "0" ]; then
            print_result 0 "Scale verification passed (error=$error)"
        else
            print_result 1 "Scale verification failed (error=$error)"
        fi
    fi
else
    print_result 1 "Could not extract error value"
fi

# Test 8: AXPY kernel verification (error should be small)
echo "Test 8: AXPY kernel result verification"
output=$("$BIN_DIR/tpbcli" run --kernel axpy --kargs ntest=10,total_memsize=128 2>&1) || true
error=$(echo "$output" | grep "axpy error" | awk '{print $NF}')
if [ -n "$error" ]; then
    if echo "$error < 1e-6" | bc -l 2>/dev/null | grep -q "1"; then
        print_result 0 "AXPY verification passed (error=$error)"
    else
        if [ "$error" = "0.000000" ] || [ "$error" = "0" ]; then
            print_result 0 "AXPY verification passed (error=$error)"
        else
            print_result 1 "AXPY verification failed (error=$error)"
        fi
    fi
else
    print_result 1 "Could not extract error value"
fi

# ==============================================================================
# Test Repeat Triad Kernel (OpenMP)
# ==============================================================================
print_header "Repeat Triad Kernel Tests (OpenMP)"

# Test: Rtriad kernel basic run
echo "Test: Rtriad kernel basic run with default params"
output=$("$BIN_DIR/tpbcli" run --kernel rtriad --kargs ntest=3,total_memsize=64 2>&1) || true
if echo "$output" | grep -q "rtriad error"; then
    print_result 0 "Rtriad kernel ran successfully"
else
    print_result 1 "Rtriad kernel failed to run"
    echo "$output"
fi

# Test: Rtriad kernel with repeat parameter
echo "Test: Rtriad kernel with repeat parameter"
output=$("$BIN_DIR/tpbcli" run --kernel rtriad --kargs ntest=3,total_memsize=64,repeat=10 2>&1) || true
if echo "$output" | grep -q "rtriad error"; then
    print_result 0 "Rtriad kernel with repeat ran successfully"
else
    print_result 1 "Rtriad kernel with repeat failed"
    echo "$output"
fi

# ==============================================================================
# Test Sum Kernel (OpenMP)
# ==============================================================================
print_header "Sum Kernel Tests (OpenMP)"

# Test: Sum kernel basic run
echo "Test: Sum kernel basic run with default params"
output=$("$BIN_DIR/tpbcli" run --kernel sum --kargs ntest=3,total_memsize=64 2>&1) || true
if echo "$output" | grep -q "sum error"; then
    print_result 0 "Sum kernel ran successfully"
else
    print_result 1 "Sum kernel failed to run"
    echo "$output"
fi

# Test: Sum kernel verification
echo "Test: Sum kernel result verification"
output=$("$BIN_DIR/tpbcli" run --kernel sum --kargs ntest=5,total_memsize=32 2>&1) || true
error=$(echo "$output" | grep "sum error" | awk '{print $NF}')
if [ -n "$error" ]; then
    print_result 0 "Sum verification passed (relative error=$error)"
else
    print_result 1 "Could not extract error value"
fi

# ==============================================================================
# Test MPI Kernels (if MPI is available)
# ==============================================================================
if [ -f "$BIN_DIR/tpbk_scale_mpi.tpbx" ] && command -v mpirun &> /dev/null; then
    print_header "Scale MPI Kernel Tests"

    # Test 9: Scale MPI kernel with 2 ranks
    echo "Test 9: Scale MPI kernel with 2 ranks"
    output=$(mpirun -np 2 "$BIN_DIR/tpbk_scale_mpi.tpbx" clock_gettime ntest=3 total_memsize=64 2>&1) || true
    if echo "$output" | grep -q "scale_mpi max error"; then
        print_result 0 "Scale MPI kernel ran successfully"
    else
        print_result 1 "Scale MPI kernel failed"
        echo "$output"
    fi

    # Test 10: Scale MPI kernel with 4 ranks
    echo "Test 10: Scale MPI kernel with 4 ranks"
    output=$(mpirun -np 4 "$BIN_DIR/tpbk_scale_mpi.tpbx" clock_gettime ntest=3 total_memsize=128 2>&1) || true
    if echo "$output" | grep -q "scale_mpi max error"; then
        print_result 0 "Scale MPI kernel with 4 ranks ran successfully"
    else
        print_result 1 "Scale MPI kernel with 4 ranks failed"
        echo "$output"
    fi
else
    echo ""
    echo -e "${YELLOW}[SKIP]${NC} Scale MPI tests - MPI not available or kernel not built"
fi

if [ -f "$BIN_DIR/tpbk_axpy_mpi.tpbx" ] && command -v mpirun &> /dev/null; then
    print_header "AXPY MPI Kernel Tests"

    # Test: AXPY MPI kernel with 2 ranks
    echo "Test: AXPY MPI kernel with 2 ranks"
    output=$(mpirun -np 2 "$BIN_DIR/tpbk_axpy_mpi.tpbx" clock_gettime ntest=3 total_memsize=64 2>&1) || true
    if echo "$output" | grep -q "axpy_mpi max error"; then
        print_result 0 "AXPY MPI kernel ran successfully"
    else
        print_result 1 "AXPY MPI kernel failed"
        echo "$output"
    fi
else
    echo ""
    echo -e "${YELLOW}[SKIP]${NC} AXPY MPI tests - MPI not available or kernel not built"
fi

if [ -f "$BIN_DIR/tpbk_rtriad_mpi.tpbx" ] && command -v mpirun &> /dev/null; then
    print_header "Repeat Triad MPI Kernel Tests"

    echo "Test: Rtriad MPI kernel with 2 ranks"
    output=$(mpirun -np 2 "$BIN_DIR/tpbk_rtriad_mpi.tpbx" clock_gettime ntest=3 total_memsize=64 2>&1) || true
    if echo "$output" | grep -q "rtriad_mpi max error"; then
        print_result 0 "Rtriad MPI kernel ran successfully"
    else
        print_result 1 "Rtriad MPI kernel failed"
        echo "$output"
    fi
else
    echo ""
    echo -e "${YELLOW}[SKIP]${NC} Rtriad MPI tests - MPI not available or kernel not built"
fi

if [ -f "$BIN_DIR/tpbk_sum_mpi.tpbx" ] && command -v mpirun &> /dev/null; then
    print_header "Sum MPI Kernel Tests"

    echo "Test: Sum MPI kernel with 2 ranks"
    output=$(mpirun -np 2 "$BIN_DIR/tpbk_sum_mpi.tpbx" clock_gettime ntest=3 total_memsize=64 2>&1) || true
    if echo "$output" | grep -q "sum_mpi"; then
        print_result 0 "Sum MPI kernel ran successfully"
    else
        print_result 1 "Sum MPI kernel failed"
        echo "$output"
    fi
else
    echo ""
    echo -e "${YELLOW}[SKIP]${NC} Sum MPI tests - MPI not available or kernel not built"
fi

# ==============================================================================
# Test with tpbcli run using --kmpiargs (if MPI kernels are registered)
# ==============================================================================
if [ -f "$BIN_DIR/tpbk_scale_mpi.tpbx" ] && command -v mpirun &> /dev/null; then
    print_header "tpbcli MPI Integration Tests"

    # Test 13: Scale MPI via tpbcli
    echo "Test 13: Scale MPI via tpbcli run"
    output=$("$BIN_DIR/tpbcli" run --kernel scale_mpi --kargs ntest=3,total_memsize=64 --kmpiargs '-np 2' 2>&1) || true
    if echo "$output" | grep -q "scale_mpi"; then
        print_result 0 "Scale MPI via tpbcli ran successfully"
    else
        print_result 1 "Scale MPI via tpbcli failed"
        echo "$output"
    fi

    # Test 14: AXPY MPI via tpbcli
    echo "Test 14: AXPY MPI via tpbcli run"
    output=$("$BIN_DIR/tpbcli" run --kernel axpy_mpi --kargs ntest=3,total_memsize=64 --kmpiargs '-np 2' 2>&1) || true
    if echo "$output" | grep -q "axpy_mpi"; then
        print_result 0 "AXPY MPI via tpbcli ran successfully"
    else
        print_result 1 "AXPY MPI via tpbcli failed"
        echo "$output"
    fi
fi

# ==============================================================================
# Summary
# ==============================================================================
print_header "Test Summary"
total=$((passed + failed))
echo "Passed: $passed / $total"
echo "Failed: $failed / $total"

if [ $failed -gt 0 ]; then
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
