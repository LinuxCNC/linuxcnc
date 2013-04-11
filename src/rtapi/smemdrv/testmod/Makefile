obj-m := test_embshmem.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement -I$(PWD)/..
KBUILD_EXTRA_SYMBOLS := $(PWD)/../Module.symvers
KERNEL_SOURCE = /usr/src/linux-headers-3.5.7-xenomai-2.6.2.1


all:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C  $(KERNEL_SOURCE)  SUBDIRS=$(PWD) clean
	
