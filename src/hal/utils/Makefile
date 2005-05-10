# Makefile for HAL Utilities

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../../Makefile.inc
endif

obj-m := scope_rt.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
# normal part of the makefile

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)

# Public headers
HEADERS =

# Private headers
PRIVHDRS = \
miscgtk.h \
scope_shm.h \
scope_rt.h \
scope_usr.h

SRCS = \
halcmd.c \
miscgtk.c \
meter.c \
scope.c \
scope_horiz.c \
scope_vert.c \
scope_trig.c \
scope_disp.c

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
scope_rt.c

LIBS = \
$(RTLIB_DIR)/scope_rt.o

endif

ifdef GTK_VERSION
GTK_BINS = \
$(BIN_DIR)/halmeter \
$(BIN_DIR)/halscope
endif

BINS = \
$(BIN_DIR)/halcmd \
$(GTK_BINS)

all : headers $(LIBS) $(BINS) modules modules_install

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

# rule to link halcmd
$(BIN_DIR)/halcmd : $(TMP_DIR)/halcmd.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o $(LIB_DIR)/libnml.a
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halmeter

HALMETER_OBJS = \
$(TMP_DIR)/meter.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halmeter : $(HALMETER_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halscope

HALSCOPE_OBJS = \
$(TMP_DIR)/scope.o \
$(TMP_DIR)/scope_horiz.o \
$(TMP_DIR)/scope_vert.o \
$(TMP_DIR)/scope_trig.o \
$(TMP_DIR)/scope_disp.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halscope : $(HALSCOPE_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean kclean

endif
