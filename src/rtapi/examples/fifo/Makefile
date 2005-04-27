# Makefile for fifo example

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS = common.h

# Sources
SRCS = \
fifousr.c

RT_SRCS = \
fifotask.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/fifotask.o

# Binaries to be created
BINS = \
$(BIN_DIR)/fifousr


all : headers $(LIBS) $(BINS)

# Generic rules
include $(SRC_DIR)/Make.rules

# explicit rule to link user space binary fifomusr
$(BIN_DIR)/fifousr : $(TMP_DIR)/fifousr.o $(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all headers depend indent install clean

