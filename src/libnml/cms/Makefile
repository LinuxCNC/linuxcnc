# Makefile for the CMS (Communication Management System) modules of the
# RCS Library

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../Makefile.inc

SRCS =	\
	cms.cc \
	cms_aup.cc \
	cms_cfg.cc \
	cms_dup.cc \
	cms_in.cc \
	cms_pm.cc \
	cms_srv.cc \
	cms_up.cc \
	cms_xup.cc \
	cmsdiag.cc \
	tcp_opts.cc \
	tcp_srv.cc \

HEADERS = \
	cms.hh \
	cms_aup.hh \
	cms_cfg.hh \
	cms_dup.hh \
	cms_srv.hh \
	cms_up.hh \
	cms_user.hh \
	cms_xup.hh \
	cmsdiag.hh \
	tcp_opts.hh \
	tcp_srv.hh \

OBJS := $(CMS_OBJS)

all: $(HEADERS) $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
