include ../config.mk
include Kbuild

include $(MODINC)

ifeq ($(BUILDSYS),kbuild)

# dirty workaround to get the RTAI directory
RTAIINCDIR = $(subst /rtai.h,,$(firstword $(wildcard $(foreach i,$(subst -I,,$(filter -I%,$(RTFLAGS))), $(i)/rtai.h))))
ifneq ($(RTAIINCDIR),)
  RTAIDIR = $(realpath $(RTAIINCDIR)/..)
endif

all:
	$(MAKE) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(RTLIBDIR)/Module.symvers $(RTAIDIR)/modules/ethercat/Module.symvers" -C $(KERNELDIR) SUBDIRS=`pwd` CC=$(CC) V=0 modules

else

ifeq ($(MODINC_HAS_EXTRA_LDFLAGS),y)
  LDFLAGS += -Wl,-rpath,$(LIBDIR)
  EXTRA_LDFLAGS += -L$(LIBDIR) -llinuxcnchal -lethercat -lrt
else
  LDFLAGS += -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -llinuxcnchal -lethercat
endif

# auto-generate header dependencies
EXTRA_CFLAGS += -MMD -MP
-include $(lcec-objs:.o=.d)

all: modules

endif

