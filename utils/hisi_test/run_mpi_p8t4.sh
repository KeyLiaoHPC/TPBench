#!/bin/bash
# !!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!
# This is a script ONLY for HiSi next gen chip.
# 1) Read the whole script to confirm your test
# purpose.
# 2) Wrong option may lead to memory overload.
# =============================================

# Build
make SETUP=tsv110_hybrid clean
make SETUP=tsv110_hybrid

# Setting
TSTAMP=`date +%Y%m%dT%H%M%S`

# Run
for s in 32 128 256 384 768 824 1024 
do
    mpirun -np 8 -x OMP_NUM_THREADS=4 -x OMP_PROC_BIND=CLOSE \
        -x OMP_PLACES=cores --map-by l3cache:PE=4 --bind-to core ./tpbench.x \
	    -k d_init,d_sum,d_copy,d_update,d_triad,d_axpy,d_striad,d_staxpy,d_scale \
        -n 10000 -s $s | tee tpbench_hi36_p8t4_${TSTAMP}.log

# This commented run instruction should be considered when running serious multi-
# core or multi-node evluations. 
# mpirun -np 1 -mca pml ucx --mca btl ^vader,tcp,openib,uct –x\
#     --map-by core --bind-to core ./tpbench.x \
#     -k d_init,d_sum,d_copy,d_update,d_triad,d_axpy,d_striad,d_staxpy,d_scale \
#     -n 10000 -s $i; \
done