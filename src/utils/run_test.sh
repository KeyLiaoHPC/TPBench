#!/bin/bash -x

# Compile
CC=mpicc

# Test parameters
nwarm=1000
npass=2
ntest=1000

# Test cases
tbase=2600000
tvdistro=NORMAL
v1=0.015
v2=2
tmethod=CGT

# Flush size
sfsize=1024
rfsize=1024
sfkern=ADD
rfkern=ADD

# Runtime
np=64
freq=2.6

# Loop over different sizes
for s in 32768 65536 131072 262144 524288 1048576 2097152 4194304
do
    for tm in ASM CGT PAPI PAPIX6 LIKWID
    do
        for tb in 2600000 26000000 260000000
        do
            for sf in COPY ADD POW DGEMM BCAST
            do
                for rf in COPY ADD POW DGEMM BCAST  
                do
                    tvdistro=NORMAL
                    v1=0.015
                    echo "Testing $s"
                    if [ $tm == "LIKWID" ]; then
                        TM_FLAGS="-I${LIKWID}/include -L${LIKWID}/lib -llikwid"
                    elif [ $tm == "PAPI" || $tm == "PAPIX6" ]; then
                        TM_FLAGS="-I${PAPI}/include -L${PAPI}/lib -lpapi"
                    fi
                    $CC -O2 -fno-builtin \
                        -DSFSIZE=$s -DRFSIZE=$s -DSF_${sf} -DRF_${rf} \
                        -DNWARM=$nwarm -DNPASS=$npass -DNTEST=$ntest -DTBASE=$tb \
                        -D${tvdistro} -DV1=$v1 -DV2=$v2 \
                        -DUSE_${tm} -o dsub.x ../dsub.c \
                        -lm \
                        $TM_FLAGS

                    mpirun -np $np --map-by core --bind-to core ./dsub.x ${freq}

                    mkdir -p ${tm}+fsize${s}+sf${sf}+rf${rf}+base${tb}+${tvdistro}+${v1}
                    mv *.log ${tm}+fsize${s}+sf${sf}+rf${rf}+base${tb}+${tvdistro}+${v1}
                    mv *.csv ${tm}+fsize${s}+sf${sf}+rf${rf}+base${tb}+${tvdistro}+${v1}

                    tvdistro=PARETO
                    v1=26
                    echo "Testing $s"
                    
                    $CC -O2 -fno-builtin \
                        -DSFSIZE=$s -DRFSIZE=$s -DSF_${sf} -DRF_${rf} \
                        -DNWARM=$nwarm -DNPASS=$npass -DNTEST=$ntest -DTBASE=$tb \
                        -D${tvdistro} -DV1=$v1 -DV2=$v2 \
                        -DUSE_${tm} -o dsub.x ../dsub.c \
                        -lm \
                        -I${PAPI}/include -L${PAPI}/lib -lpapi \
                        -I${GSL}/include -L${GSL}/lib -lgsl \
                        -L${OPENBLAS}/lib -lopenblas -lm 

                    mpirun -np $np --map-by core --bind-to core ./dsub.x ${freq}

                    mkdir -p ${tm}+fsize${s}+sf${sf}+rf${rf}+base${tb}+${tvdistro}+${v1}
                    mv *.log ${tm}+fsize${s}+sf${sf}+rf${rf}+base${tb}+${tvdistro}+${v1}
                    mv *.csv ${tm}+fsize${s}+sf${sf}+rf${rf}+base${tb}+${tvdistro}+${v1}
                done
            done
        done
    done
done
