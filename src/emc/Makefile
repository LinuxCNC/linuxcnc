# Makefile for emc directory
# This makefile calls makes in subdirectories

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../Makefile.inc
endif

obj-m := motmod.o

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
motmod-objs += log/emcmotlog.o 
motmod-objs += ../libnml/posemath/_posemath.o
motmod-objs += ../libnml/posemath/sincos.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../Makefile.inc

# Public headers
HEADERS = \
global_defs.h

# Generic rules
include $(SRC_DIR)/Make.rules

ifndef KDIR
KDIR := /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)


all: headers modules modules_install
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)


headers: def_headers
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

indent: def_indent
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

depend: def_depend
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

clean: def_clean
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

install:
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)


# Only on kbuild build systems is it required to do the 'make modules'
# all older system build the traditional way
ifeq ($(BUILD_SYS),kbuild)

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules

modules_install: modules
	- cp *.ko $(RTLIB_DIR)/
# for now we skip the proper way of installing the modules, 
# instead we just copy the modules to $(RTLIB_DIR)
#
# $(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
# depmod 2>/dev/null

else

modules:

modules_install:

endif

.PHONY: all clean depend headers indent install

endif