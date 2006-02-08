# Makefile for emc directory
# This makefile calls makes in subdirectories 

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile
# This part is responsible for building motmod.ko on 2.6 systems
# it is included here as a number of objects get linked for motmod.ko
# most included under emc/

ifeq ($(RTFLAGS),)
# We need $(PWD) as this file is read by kbuild from within the kernel sourse dir
include $(PWD)/../Makefile.inc
endif

obj-$(CONFIG_MOTMOD) := motmod.o

motmod-objs := kinematics/cubic.o 
motmod-objs += kinematics/mmxavg.o 
motmod-objs += kinematics/tc.o 
motmod-objs += kinematics/tp.o 
motmod-objs += kinematics/trivkins.o
motmod-objs += motion/motion.o 
motmod-objs += motion/command.o 
motmod-objs += motion/control.o 
motmod-objs += motion/emcmotglb.o 
motmod-objs += motion/emcmotutil.o 
motmod-objs += ../libnml/posemath/_posemath.o
motmod-objs += ../libnml/posemath/sincos.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
# Normal (as in pre-kbuild) Makefile
# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../Makefile.inc

# Public headers
HEADERS =

ifndef KDIR
KDIR := /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)

all: headers modules
	$(MAKE) -C ini $@
	$(MAKE) -C nml_intf $@
	$(MAKE) -C rs274ngc $@
	$(MAKE) -C canterp $@
	$(MAKE) -C kinematics $@
	$(MAKE) -C motion $@
	$(MAKE) -C iotask $@
	$(MAKE) -C task $@
	$(MAKE) -C usr_intf $@


headers: def_headers
	$(MAKE) -C ini $@
	$(MAKE) -C nml_intf $@
	$(MAKE) -C rs274ngc $@
	$(MAKE) -C canterp $@
	$(MAKE) -C kinematics $@
	$(MAKE) -C motion $@
	$(MAKE) -C iotask $@
	$(MAKE) -C task $@
	$(MAKE) -C usr_intf $@

indent: def_indent
	$(MAKE) -C ini $@
	$(MAKE) -C nml_intf $@
	$(MAKE) -C rs274ngc $@
	$(MAKE) -C canterp $@
	$(MAKE) -C kinematics $@
	$(MAKE) -C motion $@
	$(MAKE) -C iotask $@
	$(MAKE) -C task $@
	$(MAKE) -C usr_intf $@

depend: def_depend
	$(MAKE) -C ini $@
	$(MAKE) -C nml_intf $@
	$(MAKE) -C rs274ngc $@
	$(MAKE) -C canterp $@
	$(MAKE) -C kinematics $@
	$(MAKE) -C motion $@
	$(MAKE) -C iotask $@
	$(MAKE) -C task $@
	$(MAKE) -C usr_intf $@

clean: def_clean kclean
	$(MAKE) -C ini $@
	$(MAKE) -C nml_intf $@
	$(MAKE) -C rs274ngc $@
	$(MAKE) -C canterp $@
	$(MAKE) -C kinematics $@
	$(MAKE) -C motion $@
	$(MAKE) -C iotask $@
	$(MAKE) -C task $@
	$(MAKE) -C usr_intf $@

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions

install:
	$(MAKE) -C ini $@
	$(MAKE) -C nml_intf $@
	$(MAKE) -C rs274ngc $@
	$(MAKE) -C canterp $@
	$(MAKE) -C kinematics $@
	$(MAKE) -C motion $@
	$(MAKE) -C iotask $@
	$(MAKE) -C task $@
	$(MAKE) -C usr_intf $@

# Generic rules
include $(SRC_DIR)/Make.rules

# Only on kbuild build systems is it required to do the 'make modules'
# all older system build the traditional way
ifeq ($(BUILD_SYS),kbuild)

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules
	- cp *.ko $(RTLIB_DIR)/

modules_install: modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null

else

modules:

modules_install:

endif

.PHONY: all clean kclean depend headers indent install modules

endif
