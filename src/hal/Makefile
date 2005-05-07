# Makefile for HAL
include $(PWD)/../Makefile.inc

ifneq ($(KERNELRELEASE),)
# kbuild part of the makefile

ifeq ($(RTFLAGS),)
endif

obj-m := hal_lib.o

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

else
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

# Generic rules
include $(SRC_DIR)/Make.rules

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

modules_install: modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

headers: def_headers
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

indent: def_indent
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

depend: def_depend
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

clean :  def_clean kclean
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

kclean:
	- rm -f .*.cmd *.o* *.mod.* *.ko
	- rm -fR .tmp_versions 


.PHONY: all headers depend indent install clean

endif