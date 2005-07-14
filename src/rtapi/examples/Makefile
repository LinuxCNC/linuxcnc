ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../../Makefile.inc
endif

obj-$(CONFIG_RTAPI_EXAMPLES_EXTINT) := extint.o
extint-objs := extint/extint.o
obj-$(CONFIG_RTAPI_EXAMPLES_FIFO) += fifotask.o
fifotask-objs := fifo/fifotask.o
obj-$(CONFIG_RTAPI_EXAMPLES_SEM) += master.o
master-objs := semaphore/master.o
obj-$(CONFIG_RTAPI_EXAMPLES_SEM) += slave.o
slave-objs := semaphore/slave.o
obj-$(CONFIG_RTAPI_EXAMPLES_SHMEM) += shmemtask.o
shmemtask-objs := shmem/shmemtask.o
obj-$(CONFIG_RTAPI_EXAMPLES_TIMER) += timertask.o
timertask-objs := timer/timertask.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
# normal part of the makefile

# Makefile for RTAPI examples
#
# Note: this file makes nothing itself, it merely changes to the
# individual example directories and runs make there
#
include $(PWD)/../../Makefile.inc

ifndef KDIR
KDIR := /lib/modules/$(shell uname -r)/build
endif

all headers depend indent install: modules
	(cd extint; make $@)
	(cd timer; make $@)
	(cd shmem; make $@)
	(cd semaphore; make $@)
	(cd fifo; make $@)

clean: kclean
	(cd extint; make $@)
	(cd timer; make $@)
	(cd shmem; make $@)
	(cd semaphore; make $@)
	(cd fifo; make $@)

# Only on kbuild build systems is it required to do the 'make modules'
# all older system build the traditional way
ifeq ($(BUILD_SYS),kbuild)

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules
	- cp *.ko $(RTLIB_DIR)/

modules_install: modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions

else

modules:

modules_install:

kclean:

endif

.PHONY : all headers depend indent install clean kclean modules

endif