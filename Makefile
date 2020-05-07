#!/bin/sh

include setup/Make.${SETUP}

VPATH = ./src

.PHONY: clean

bench.x: bench_var.h main.c kernels.c utils.c
	$(CC) $(CFLAGS) $(DEF) $(SIZE) -o $@ $^

clean:
	-rm -f *.x *.o

