#!/bin/bash -x

# Compile
CC=mpicc

# Test parameters
nwarm=1000
npass=2
ntest=100

# Test cases
tbase=100
# Theoretical distribution type
tvdistro=UNIFORM
# Theoretical distribution parameters
# Uniform: V1: Number of DSub between two bins; V2: Number of bins.
# Normal: V1: sigma, standard error
# Pareto: V1: alpha, Pareto exponent 
v1=100
# Uniform: Number of intervals (>=1)
v2=2

# Timing method
tm=CGT

# Flush size before and after kernel in Bytes
sfsize=1024
rfsize=1024
sfkern=ADD
rfkern=ADD

# Runtime
np=64
# Use this only for RDTSC or CNTVCT which convert tick to ns
freq=260000000
FREQ_GHZ=2.9

if [ $tm == "LIKWID" ]; then
    TM_FLAGS="-I${LIKWID}/include -L${LIKWID}/lib -llikwid"
elif [ $tm == "PAPI" ] || [ $tm == "PAPIX6" ]; then
    TM_FLAGS="-I${PAPI}/include -L${PAPI}/lib -lpapi"
fi

$CC -O2 -fno-builtin \
    -DSFSIZE=$sfsize -DRFSIZE=$rfsize -DSF_${sfkern} -DRF_${rfkern} \
    -DNWARM=$nwarm -DNPASS=$npass -DNTEST=$ntest -DTBASE=$tbase \
    -D${tvdistro} -DV1=$v1 -DV2=$v2 -DNP=$np -DFREQ=$freq \
    -DUSE_${tm} -o get_time_error.x ./get_time_error.c \
    -lm \
    $TM_FLAGS
mpirun -np $np --map-by core --bind-to core ./get_time_error.x ${freq}
python3 ./calc_w.py 100 99 90 $FREQ_GHZ .
bak_dir=check_time_error_`date +%Y%m%d%H%M%S`
mkdir $bak_dir
mv *.csv $bak_dir
mv *.log $bak_dir
