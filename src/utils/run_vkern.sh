#!/bin/bash -x

# Compile
CC=mpicc

# Test parameters
nwarm=1000
npass=2
ntest=100

# Test cases
tbase=100
tvdistro=UNIFORM
v1=100
v2=2
tm=ASM

# Flush size
sfsize=1024
rfsize=1024
sfkern=ADD
rfkern=ADD

# Runtime
np=64
freq=260000000

if [ $tm == "LIKWID" ]; then
    TM_FLAGS="-I${LIKWID}/include -L${LIKWID}/lib -llikwid"
elif [ $tm == "PAPI" || $tm == "PAPIX6" ]; then
    TM_FLAGS="-I${PAPI}/include -L${PAPI}/lib -lpapi"
fi

$CC -O2 -fno-builtin \
    -DSFSIZE=$sfsize -DRFSIZE=$rfsize -DSF_${sfkern} -DRF_${rfkern} \
    -DNWARM=$nwarm -DNPASS=$npass -DNTEST=$ntest -DTBASE=$tbase \
    -D${tvdistro} -DV1=$v1 -DV2=$v2 -DNP=$np -DFREQ=$freq \
    -DUSE_${tm} -o dsub.x ./dsub.c \
    -lm \
    $TM_FLAGS

mpirun -np $np --map-by core --bind-to core ./dsub.x ${freq}
