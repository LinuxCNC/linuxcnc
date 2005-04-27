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

RT_SRCS = \
shmemtask.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/shmemtask.o

BINS = \
$(BIN_DIR)/shmemusr

all : headers $(LIBS) $(BINS)

# Generic rules
include $(SRC_DIR)/Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

# explicit to link user space binary shmemusr
$(BIN_DIR)/shmemusr : $(TMP_DIR)/shmemusr.o $(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean

