# Makefile for the CMS (Communication Management System) modules of the
# RCS Library

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../Makefile.inc

SRCS =	cms.cc \
	cms_cfg.cc \
	cms_in.cc \
	cms_up.cc

HEADERS = cms.hh \
	cms_cfg.hh \
	cms_up.hh \
	cms_user.hh

OBJS := $(CMS_OBJS)

all: $(HEADERS) $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
