# Makefile for the CMS (Communication Management System) modules of the
# RCS Library

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../Makefile.inc

SRCS =	\
	locmem.cc \
	memsem.cc \
	phantom.cc \
	physmem.cc \
	recvn.c \
	sendn.c \
	shmem.cc \
	tcpmem.cc \

HEADERS = \
	locmem.hh \
	memsem.hh \
	phantom.hh \
	physmem.hh \
	recvn.h \
	rem_msg.hh \
	sendn.h \
	shmem.hh \
	tcpmem.hh \

LOCAL_CFLAGS = -fPIC
LOCAL_CXXFLAGS = -fPIC

OBJS := $(CMS_OBJS)

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
