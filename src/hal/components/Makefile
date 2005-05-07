# Makefile for HAL Components

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../../Makefile.inc
endif

obj-m := blocks.o
obj-m += debounce.o
obj-m += encoder.o
obj-m += encoder_ratio.o
obj-m += stepgen.o
obj-m += freqgen.o
obj-m += siggen.o
obj-m += pid.o
obj-m += supply.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
# normal part of the makefile

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
blocks.c \
debounce.c \
encoder.c \
encoder_ratio.c \
stepgen.c \
freqgen.c \
siggen.c \
pid.c \
supply.c

LIBS = \
$(RTLIB_DIR)/blocks.o \
$(RTLIB_DIR)/debounce.o \
$(RTLIB_DIR)/encoder.o \
$(RTLIB_DIR)/encoder_ratio.o \
$(RTLIB_DIR)/stepgen.o \
$(RTLIB_DIR)/freqgen.o \
$(RTLIB_DIR)/siggen.o \
$(RTLIB_DIR)/pid.o \
$(RTLIB_DIR)/supply.o

endif

all : headers $(LIBS) modules

# Generic rules
include $(SRC_DIR)/Make.rules

ifneq ($(MODULE_EXT),.ko)

# specific rule to link siggen with libm
$(RTLIB_DIR)/siggen.o: $(RTTMP_DIR)/siggen.o
	@ echo "Linking kernel module $@"
	ld -r -static -S -O2 $^ $(MATHLIB) -o $@

endif

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


modules: headers
# Only on kbuild build systems is it required to do the 'make modules'
# all older system build the traditional way
ifeq ($(BUILD_SYS),kbuild)
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules

modules_install: modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null

endif

.PHONY: all headers depend indent install clean

endif