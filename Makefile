#!/bin/sh

include setup/Make.${SETUP}

TP_DIR = $(PWD)
SRC = $(TP_DIR)/src
KERNELS = $(SRC)/kernels
GROUPS = $(SRC)/groups
INC = $(SRC)/include
VPATH = $(SRC):$(KERNELS):$(KERNELS)/asm:$(KERNELS)/blas1:$(KERNELS)/simple:${KERNELS}/stencil2d:${KERNELS}/blas3:$(GROUPS)

.PHONY: clean test 

tpbench.x:	main.c tpmpi.c tpb_core.c cli_parser.c tpio.c tpdata.c tplog.c init.c staxpy.c striad.c sum.c triad.c \
			update.c axpy.c copy.c scale.c cli_parser.c stream.c stream_verbose.c tl_cgw.c jacobi5p.c mulldr.c \
			fmaldr.c rtriad.c gemm_bcast.c gemm_allreduce.c jacobi2d5p_sendrecv.c
			$(CC) $(CFLAGS) -I$(INC) -o $@ $^

test: test.x
test.x: test.c init.c staxpy.c striad.c sum.c triad.c update.c \
		axpy.c copy.c scale.c cli_parser.c
	$(CC) -g $(CFLAGS) -I$(INC) -o $@ $^

clean:
	rm -f *.x *.o

