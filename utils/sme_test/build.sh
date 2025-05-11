#!/bin/bash
gcc -march=armv9.a+sme-f64f64 -O3 ./sme_correctness.c -o sme_correctness.x
gcc -march=armv9.a+sme-f64f64 -O3 ./ibench.c -o ibench.x
