# Makefile for HAL Virtual Control Panel

# normal part of the makefile

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)

# Public headers
HEADERS =

# Private headers
PRIVHDRS = \
tokenizer.h \
vcp.h

SRCS = \
tokenizer.c \
vcp_main.c \
vcp_widgets.c

ifdef GTK_VERSION
GTK_BINS = \
$(BIN_DIR)/halvcp
endif

BINS = \
$(GTK_BINS)

all : headers $(LIBS) $(BINS)

# Generic rules
include $(SRC_DIR)/Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean :  def_clean

# rule to link halvcp

VCP_OBJS = \
$(TMP_DIR)/tokenizer.o \
$(TMP_DIR)/vcp_main.o \
$(TMP_DIR)/vcp_widgets.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halvcp : $(VCP_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean

