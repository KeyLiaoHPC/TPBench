#!/bin/sh

include setup/Make.${SETUP}

TP_DIR = $(HOME)/proj/TPBench
SRC = $(TP_DIR)/src
KERNELS = $(SRC)/kernels
INC = $(SRC)/include
VPATH = $(SRC):$(KERNELS):$(KERNELS)/asm:$(KERNELS)/blas1:$(KERNELS)/simple

.PHONY: clean test 

tpbench.x:	main.c tpb_core.c cli_parser.c init.c staxpy.c striad.c sum.c triad.c \
			update.c axpy.c copy.c scale.c cli_parser.c
			$(CC) -g $(CFLAGS) -I$(INC) -o $@ $^

test: test.x
test.x: test.c init.c staxpy.c striad.c sum.c triad.c update.c \
		axpy.c copy.c scale.c cli_parser.c
	$(CC) -g $(CFLAGS) -I$(INC) -o $@ $^

clean:
	-rm -f *.x *.o

