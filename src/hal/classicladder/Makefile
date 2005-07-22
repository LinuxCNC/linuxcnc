# Makefile for HAL Components

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../../Makefile.inc
endif

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

ifdef SEQUENTIAL_SUPPORT
EXTRA_CFLAGS += -DSEQUENTIAL_SUPPORT
endif

obj-$(CONFIG_CLASSICLADDER_RT) := classicladder_rt.o
classicladder_rt-objs := module_rt.o
classicladder_rt-objs += calc_rt.o
classicladder_rt-objs += vars_access.o
classicladder_rt-objs += arithm_eval.o
classicladder_rt-objs += manager.o
classicladder_rt-objs += arrays.o

ifdef SEQUENTIAL_SUPPORT
classicladder_rt-objs += calc_sequential_rt.o
endif

else
# normal part of the makefile

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

GTK_CFLAGS += -DGTK_INTERFACE

ifeq ($(GTK_VERSION), 1.2)
GTK_CFLAGS += -DNO_GTK_STYLE_GET_FONT
endif

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)
LOCAL_CFLAGS += -fstrict-aliasing -Wno-strict-aliasing

ifdef SEQUENTIAL_SUPPORT
LOCAL_CFLAGS += -DSEQUENTIAL_SUPPORT
LOCAL_RTFLAGS += -DSEQUENTIAL_SUPPORT
endif

# Public headers
HEADERS =

# Private headers
PRIVHDRS = \
arrays.h \
arithm_eval.h \
calc.h \
calc_rt.h \
calc_sequential.h \
calc_sequential_rt.h \
classicladder_gtk.h \
classicladder.h \
config_gtk.h \
drawing.h \
drawing_sequential.h \
edit_gtk.h \
edit.h \
editproperties_gtk.h \
edit_sequential.h \
files.h \
files_sequential.h \
global.h \
manager.h \
manager_gtk.h \
protocol_modbus_slave.h \
sequential.h \
socket_server.h \
vars_access.h

SRCS = \
arithm_eval.c \
arrays.c \
calc.c \
calc_sequential.c \
classicladder.c \
classicladder_gtk.c \
config_gtk.c \
drawing.c \
drawing_sequential.c \
edit.c \
edit_gtk.c \
editproperties_gtk.c \
edit_sequential.c \
files.c \
files_sequential.c \
manager.c \
manager_gtk.c \
protocol_modbus_slave.c \
socket_server.c \
vars_access.c

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
module_rt.c \
calc_rt.c \
calc_sequential_rt.c \
vars_access.c \
arithm_eval.c \
manager.c \
arrays.c

LIBS = \
$(RTLIB_DIR)/classicladder_rt.o

endif

ifdef GTK_VERSION
GTK_BINS = \
$(BIN_DIR)/classicladder
endif

BINS = \
$(GTK_BINS)

all : headers $(LIBS) $(BINS) modules

# Generic rules
include $(SRC_DIR)/Make.rules

ifneq ($(MODULE_EXT),.ko)

# rule to link classicladder_rt
OBJ_CLASSICLADDER_RT = \
$(RTTMP_DIR)/module_rt.o \
$(RTTMP_DIR)/calc_rt.o \
$(RTTMP_DIR)/vars_access.o \
$(RTTMP_DIR)/arithm_eval.o \
$(RTTMP_DIR)/manager.o \
$(RTTMP_DIR)/arrays.o

ifdef SEQUENTIAL_SUPPORT
OBJ_CLASSICLADDER_RT += $(RTTMP_DIR)/calc_sequential_rt.o
endif

$(RTLIB_DIR)/classicladder_rt.o: $(OBJ_CLASSICLADDER_RT)
	@ echo "Linking kernel module $@"
	ld -r -static -S -O2 $^ $(MATHLIB) -o $@

endif

headers: def_headers
indent: def_indent
depend: def_depend
clean: def_clean kclean

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
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
	- cp *.ko $(RTLIB_DIR)/

modules_install: modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null

else

modules:

modules_install:

endif


OBJ_CLASSICLADDER = \
$(TMP_DIR)/arithm_eval.o \
$(TMP_DIR)/arrays.o \
$(TMP_DIR)/calc.o \
$(TMP_DIR)/classicladder.o \
$(TMP_DIR)/files.o \
$(TMP_DIR)/manager.o \
$(TMP_DIR)/protocol_modbus_slave.o \
$(TMP_DIR)/socket_server.o \
$(TMP_DIR)/vars_access.o

ifdef SEQUENTIAL_SUPPORT
OBJ_CLASSICLADDER += $(TMP_DIR)/calc_sequential.o
OBJ_CLASSICLADDER += $(TMP_DIR)/files_sequential.o
endif

OBJ_CLASSICLADDER_GTK = \
$(TMP_DIR)/classicladder_gtk.o \
$(TMP_DIR)/config_gtk.o \
$(TMP_DIR)/drawing.o \
$(TMP_DIR)/edit.o \
$(TMP_DIR)/edit_gtk.o \
$(TMP_DIR)/editproperties_gtk.o \
$(TMP_DIR)/manager_gtk.o \

ifdef SEQUENTIAL_SUPPORT
OBJ_CLASSICLADDER_GTK += $(TMP_DIR)/drawing_sequential.o
OBJ_CLASSICLADDER_GTK += $(TMP_DIR)/edit_sequential.o
endif

OBJ_USER_LIBS = \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

# rule to link classicladder
$(BIN_DIR)/classicladder : $(OBJ_CLASSICLADDER_GTK) $(OBJ_CLASSICLADDER) $(OBJ_USER_LIBS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) -lpthread $^ -o $@

.PHONY: all headers depend indent install modules clean kclean

endif
