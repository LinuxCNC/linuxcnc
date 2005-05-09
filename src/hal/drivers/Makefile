# Makefile for HAL Drivers
#
# NOTE: To avoid possible confusion or contention with
# normal Linux drivers, all HAL drivers should begin with
# the string "hal_"
#
# The default rules in this file and Make.rules will properly
# build an driver that consists of a single source file, linked
# to the RTAPU/ULAPI and HAL libraries only.  All you need to
# do to add a new driver is to add <name>.c to SRCS and
# $(BIN_DIR)/<name> to BINS (for a user space driver), and/or
# add <name>.c to RT_SCRS and $(RTLIB_DIR)/<name>.o to LIBS (for
# a realtime driver).
#
# FIXME - can we change the default rules so they properly link
# modules that require the math library, but do _not_ link the
# math lib if it isn't needed?  If not, then modules that require
# the math lib will also require explicit rules ;-(
#
# get the defs for CC, CFLAGS, LIB_DIR, etc.

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../../Makefile.inc
endif

obj-m := hal_parport.o
obj-m += hal_tiro.o
obj-m += hal_evoreg.o
obj-m += hal_motenc.o
obj-m += hal_skeleton.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
# normal part of the makefile

include ../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS = motenc.h

SRCS = \
hal_parport.c \
hal_skeleton.c

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
hal_parport.c \
hal_tiro.c \
hal_evoreg.c \
hal_motenc.c \
hal_skeleton.c

LIBS = \
$(RTLIB_DIR)/hal_parport.o \
$(RTLIB_DIR)/hal_tiro.o \
$(RTLIB_DIR)/hal_evoreg.o \
$(RTLIB_DIR)/hal_motenc.o \
$(RTLIB_DIR)/hal_skeleton.o

endif

BINS = \
$(BIN_DIR)/hal_parport \
$(BIN_DIR)/hal_skeleton


all : headers $(OBJS) $(LIBS) $(BINS) modules modules_install

# Generic rules
include $(SRC_DIR)/Make.rules

headers: def_headers
indent: def_indent
depend: def_depend
clean :  def_clean kclean

kclean:
	- rm -f .*.cmd *.o* *.mod.* *.ko
	- rm -fR .tmp_versions 



ifndef KDIR
# this doesn't work on debian where make-kpkg was at work
#KDIR := $(KERNELDIR)
KDIR := /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)


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

# rule to link single module binaries (user space versions of drivers)
$(BIN_DIR)/% : $(TMP_DIR)/%.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking user space driver $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@


.PHONY: all headers depend indent install clean

endif