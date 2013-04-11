.PHONY: all clean build load unload
obj-m := embshmem.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement
#KERNEL_SOURCE = ~/kernel/linux-source-3.0.0
KERNEL_SOURCE = /usr/src/linux-headers-3.5.7-xenomai-2.6.2.1

all: unload build load

load: 
	sudo ./installit.sh
	cd testmod && sudo insmod test_embshmem.ko

unload:
	- sudo rmmod test_embshmem.ko
	- sudo rmmod embshmem.ko
	
build:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules
	cd testmod && $(MAKE)
	$(MAKE) -f Makefile.testprog

clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
	$(MAKE) -f Makefile.testprog clean
	$(MAKE) -C testmod clean



