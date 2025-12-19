#!/bin/bash
echo "=========================================="
echo "TPBench Argument Parsing Test Suite"
echo "=========================================="
echo ""

echo "Test 1: Basic kernel with dtype and memsize"
./tpbcgt.x run -k triad:dtype=double:memsize=128 --kargs ntest=100 2>&1 | grep -E "(tests|Elements)" | head -2
echo ""

echo "Test 2: Kernel-specific args override common args"
./tpbcgt.x run -k triad:ntest=20:memsize=256 --kargs ntest=5,memsize=32 2>&1 | grep -E "(tests|Elements)" | head -2
echo ""

echo "Test 3: Later args override earlier args"
./tpbcgt.x run -k triad:memsize=64:memsize=512 --kargs ntest=3 2>&1 | grep -E "(tests|Elements)" | head -2
echo ""

echo "Test 4: Warning for unsupported common arg (continues)"
./tpbcgt.x run -k triad:dtype=double --kargs ntest=3,unsupported=xyz 2>&1 | grep -E "(Warning|tests)" | head -2
echo ""

echo "Test 5: Error for unsupported kernel-specific arg (exits)"
./tpbcgt.x run -k triad:invalid_param=100 --kargs ntest=3 2>&1 | grep -E "(Error|FAIL)" | head -2
echo ""

echo "Test 6: Error for invalid dtype value (exits)"
./tpbcgt.x run -k triad:dtype=invalid --kargs ntest=3 2>&1 | grep -E "(Invalid|FAIL)" | head -2
echo ""

echo "Test 7: Error for nonexistent kernel (exits)"
./tpbcgt.x run -k nonexistent_kernel --kargs ntest=3 2>&1 | grep "not found"
echo ""

echo "Test 8: Simple kernel name without args"
./tpbcgt.x run -k triad --kargs ntest=3 2>&1 | grep -E "(Kernel routine|tests)" | head -3
echo ""

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
