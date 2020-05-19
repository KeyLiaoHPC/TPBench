#!/bin/sh

include setup/Make.${SETUP}

TP_DIR = $(HOME)/proj/TPBench
VPATH = $(TP_DIR)/src:$(TP_DIR)/src/kernels
.PHONY: clean test 

tpbench.x: main.c kernel_wrapper.c utils.c g1_kernel.c
	$(CC) -g $(CFLAGS) $(DEF) $(SIZE) -I$(TP_DIR)/src -o $@ $^
test: test.x
test.x: test.c kernel_wrapper.c g1_kernel.c
	$(CC) -g $(CFLAGS) -I$(TP_DIR)/src -o $@ $^

clean:
	-rm -f *.x *.o

