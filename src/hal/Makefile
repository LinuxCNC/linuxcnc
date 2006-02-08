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
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C user_comps $@
	$(MAKE) -C classicladder $@

# Generic rules
include $(SRC_DIR)/Make.rules


# Only on kbuild build systems is it required to do the 'make modules'
# all older system build the traditional way
ifeq ($(BUILD_SYS),kbuild)

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules
	- cp *.ko $(RTLIB_DIR)/
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C classicladder $@

modules_install: modules
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C classicladder $@

	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install
	depmod 2>/dev/null

else

modules:

modules_install:

endif

headers: def_headers
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C user_comps $@
	$(MAKE) -C classicladder $@

indent: def_indent
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C user_comps $@
	$(MAKE) -C classicladder $@

depend: def_depend
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C user_comps $@
	$(MAKE) -C classicladder $@

clean :  def_clean kclean
	$(MAKE) -C utils $@
	$(MAKE) -C drivers $@
	$(MAKE) -C components $@
	$(MAKE) -C user_comps $@
	$(MAKE) -C classicladder $@

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions 


.PHONY: all headers depend indent install modules clean kclean

endif
