include ../config.mk

EXTRA_CFLAGS := $(filter-out -Wframe-larger-than=%,$(EXTRA_CFLAGS))

LCEC_CONF_OBJS = \
	conf.o \
	conf_util.o \
	conf_icmds.o \

.PHONY: all clean install

all: lcec_conf

install: lcec_conf
	mkdir -p $(DESTDIR)$(EMC2_HOME)/bin
	cp lcec_conf $(DESTDIR)$(EMC2_HOME)/bin/

lcec_conf: $(LCEC_CONF_OBJS)
	$(CC) -o $@ $(LCEC_CONF_OBJS) -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -llinuxcnchal -lexpat

%.o: %.c
	$(CC) -o $@ $(EXTRA_CFLAGS) -MMD -MP -URTAPI -DULAPI -Os -c $<

-include $(LCEC_CONF_OBJS:.o=.d)

