# Makefile for shared memory task example

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc


# Public headers
HEADERS =

# Private headers
PRIVHDRS = \
common.h

# Sources
SRCS = \
shmemusr.c

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
shmemtask.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/shmemtask.o

endif

BINS = \
$(BIN_DIR)/shmemusr

all : headers $(LIBS) $(BINS)

LOCAL_CFLAGS = -fstrict-aliasing -Wno-strict-aliasing

# Generic rules
include $(SRC_DIR)/Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean kclean

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions

# explicit to link user space binary shmemusr
$(BIN_DIR)/shmemusr : $(TMP_DIR)/shmemusr.o $(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean kclean

