#!/bin/sh
CC=gcc
CFLAGS=-O3 -fno-builtin -march=armv8-a+simd -mtune=native
DEF=-DNTIMES=1000
SIZE=-DKIB_SIZE=32768

bench.x: bench_var.h main.c kernels.c utils.c
	$(CC) $(CFLAGS) $(DEF) $(SIZE) -o $@ $^

clean:
	rm -f *.x *.o
