# Makefile for HAL

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
include $(PWD)/../Makefile.inc
endif

obj-m := hal_lib.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
include ../Makefile.inc
# pre kbuild Make process

# Public headers
HEADERS = \
hal.h

# Private headers
PRIVHDRS = \
hal_priv.h

SRCS = \
hal_lib.c

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
hal_lib.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o 

endif

LIBS += \
$(LIB_DIR)/hal_lib.o

ifndef KDIR
KDIR := /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)


all : headers $(LIBS) modules
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

# Generic rules
include $(SRC_DIR)/Make.rules


# Only on kbuild build systems is it required to do the 'make modules'
# all older system build the traditional way
ifeq ($(BUILD_SYS),kbuild)

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules
	- cp *.ko $(RTLIB_DIR)/
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

modules_install: modules
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null

else

modules:

modules_install:

endif

headers: def_headers
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

indent: def_indent
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

depend: def_depend
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

clean :  def_clean kclean
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)
	(cd classicladder; make $@)

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions 


.PHONY: all headers depend indent install modules clean kclean

endif
