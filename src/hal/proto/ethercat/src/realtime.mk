include ../config.mk
include Kbuild

include $(MODINC)

LDFLAGS += -Wl,-rpath,$(LIBDIR)
EXTRA_LDFLAGS += -L$(LIBDIR) -llinuxcnchal -lethercat -lrt

# auto-generate header dependencies
EXTRA_CFLAGS += -MMD -MP
-include $(lcec-objs:.o=.d)

