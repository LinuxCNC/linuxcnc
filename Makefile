default: hy_vfd

MODINC := $(shell ./find-modinc)
BINDIR := $(shell ./find-bindir)

ifeq "$(MODINC)" ""
$(error Required files for building components not present.  Install emc2-dev)
endif
include $(MODINC)

ifeq ($(RUN_IN_PLACE),no)
EXTRA_CFLAGS += -I$(EMC2_HOME)/include/emc2
LIBDIR := $(shell ./find-libdir)
ifeq "$(LIBDIR)" ""
$(error LIBDIR not found)
endif
endif

CFLAGS := $(EXTRA_CFLAGS) -URTAPI -U__MODULE__ -DULAPI -Os
CFLAGS += $(shell pkg-config --cflags glib-2.0)
LFLAGS := -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -lemchal
LFLAGS += $(shell pkg-config --libs glib-2.0)

include .o/hy_vfd.d .o/hy_modbus.d

install: hy_vfd
	cp hy_vfd $(BINDIR)

hy_vfd: .o/hy_vfd.o .o/hy_modbus.o  -lpthread
	$(CC) -o $@ $^ $(LFLAGS)
	
.o/%.o: %.c
	mkdir -p .o
	$(CC) $(CFLAGS) -o $@ -c $<

.o/%.d: %.c
	mkdir -p .o
	$(CC) $(CFLAGS) -MM -MT "$@ $(patsubst %.d,%.o,$@)" $< -o $@.tmp \
			&& mv $@.tmp $@

clean:
	-rm -f hy_vfd
	-rm -rf .o
