# Makefile for HAL

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# local flags for GTK includes
LOCAL_CFLAGS = `gtk-config --cflags`

# Public headers
HEADERS = \
hal.h

# Private headers
PRIVHDRS = \
hal_priv.h

SRCS = \
hal_lib.c \
halcmd.c \
halmeter.c

RT_SRCS = \
hal_lib.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o \
$(LIB_DIR)/hal_lib.o

BINS = \
$(BIN_DIR)/halcmd \
$(BIN_DIR)/halmeter

all : headers $(LIBS) $(BINS)
	(cd drivers; make $@)
	(cd components; make $@)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
	(cd drivers; make $@)
	(cd components; make $@)

indent: def_indent
	(cd drivers; make $@)
	(cd components; make $@)

depend: def_depend
	(cd drivers; make $@)
	(cd components; make $@)

clean :  def_clean
	(cd drivers; make $@)
	(cd components; make $@)

# rule to link halcmd
$(BIN_DIR)/halcmd : $(TMP_DIR)/halcmd.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halmeter
$(BIN_DIR)/halmeter : $(TMP_DIR)/halmeter.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking $@"
	$(CC) `gtk-config --libs` $(CFLAGS) $(ULFLAGS) $^ -o $(BIN_DIR)/halmeter

.PHONY: all headers depend indent install clean

