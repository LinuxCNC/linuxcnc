# Makefile for HAL

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)

# Public headers
HEADERS = \
hal.h

# Private headers
PRIVHDRS = \
hal_priv.h \
halgtk.h \
#halscope.h

SRCS = \
hal_lib.c \
halcmd.c \
halmeter.c \
probe.c \
#halscope.c

RT_SRCS = \
hal_lib.c \
#halscope_rt.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o \
$(LIB_DIR)/hal_lib.o


ifdef GTK_VERSION
GTK_BINS = \
$(BIN_DIR)/halmeter \
#$(BIN_DIR)/halscope
endif

BINS = \
$(BIN_DIR)/halcmd \
$(GTK_BINS)

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
$(BIN_DIR)/halmeter : $(TMP_DIR)/halmeter.o $(TMP_DIR)/probe.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halscope
$(BIN_DIR)/halscope : $(TMP_DIR)/halscope.o $(TMP_DIR)/probe.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean

