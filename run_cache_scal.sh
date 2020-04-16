#!/bin/sh
node=tx2

for k in 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072
do
    make clean && make -f Make.gcc SIZE=-DKIB_SIZE=$k && numactl -C 22 ./bench.x
done
mkdir -p ./data/data_${node}_cache_scal
mv *.csv ./data/data_${node}_cache_scal
