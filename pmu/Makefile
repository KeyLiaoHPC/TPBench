#!/bin/sh


obj-m	:= enable_pmu.o
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

# For newer kernel >= 5, using M instead of SUBDIRS

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
