CC=gcc
CFLAGS=-O3 -fno-builtin -march=armv8-a+simd -mtune=native
DEF=-DNTIMES=100 -DKIB_SIZE=524288

bench.x: bench_var.h main.c kernels.c utils.c
	$(CC) $(CFLAGS) $(DEF) -o $@ $^

clean:
	rm -f *.x *.o
