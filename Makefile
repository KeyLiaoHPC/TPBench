#!/bin/sh

include setup/Make.${SETUP}

VPATH = ./src

.PHONY: clean

tpbench.x: main.c kernels.c utils.c 
	$(CC) -g $(CFLAGS) $(DEF) $(SIZE) -I. -o $@ $^

clean:
	-rm -f *.x *.o

