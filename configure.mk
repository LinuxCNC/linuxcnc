ifeq (, $(COMP))
  COMP = $(shell which halcompile)
endif
ifeq (, $(COMP))
  COMP = $(shell which comp)
endif

.PHONY: configure
configure:
	@echo "COMP = $(COMP)"
	@echo "MODINC = $(MODINC)"
	@echo "CC = $(CC)"
	@echo "RTFLAGS = $(RTFLAGS)"
	@echo "EXTRA_CFLAGS = $(EXTRA_CFLAGS)"
	@echo "EMC2_HOME = $(EMC2_HOME)"
	@echo "RUN_IN_PLACE = $(RUN_IN_PLACE)"
	@echo "RTLIBDIR = $(RTLIBDIR)"
	@echo "LIBDIR = $(LIBDIR)"
	@echo "prefix = $(prefix)"

# include modinc - try halcompile first, then pkg-config, then linuxcnc-config
ifeq (, $(MODINC))
  MODINC := $(shell halcompile --print-modinc 2>/dev/null)
endif
ifeq (, $(MODINC))
  MODINC := $(shell pkg-config --variable=modinc linuxcnc 2>/dev/null)
endif
ifeq (, $(MODINC))
  MODINC := $(shell linuxcnc-config --modinc 2>/dev/null)
endif
ifeq (, $(MODINC))
  $(error Unable to get modinc path)
endif

include $(MODINC)

