# Makefile for HAL

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

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

LIBS =

OBJS = \
$(RTLIB_DIR)/hal_lib.o \
$(LIB_DIR)/hal_lib.o \
$(TMP_DIR)/halcmd.o \
$(GTKTMP_DIR)/halmeter.o

BINS = $(BIN_DIR)/test_comp $(BIN_DIR)/halcmd $(BIN_DIR)/halmeter

all : headers $(OBJS) $(LIBS)
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

#$(BIN_DIR)/% : $(LIB_DIR)/%.o $(ULAPI) $(LIB_DIR)/hal_lib.o
#	@ echo "Rule 3: Linking $@"
#	$(CC) $(CFLAGS) $(ULFLAGS) $< $(ULAPI) $(LIB_DIR)/hal_lib.o -o $@
#

#special rule for GTK binaries

$(BIN_DIR)/halmeter : $(LIB_DIR)/halmeter.o $(ULAPI) $(LIB_DIR)/hal_lib.o
	@ echo "Rule 5: Linking $@ using GTK rule"
	$(CC) `gtk-config --libs` $(CFLAGS) $(ULFLAGS) $(LIB_DIR)/halmeter.o $(ULAPI) $(LIB_DIR)/hal_lib.o -o $(BIN_DIR)/halmeter


.PHONY: all headers depend indent install clean

