# -*- Makefile -*-

AM_CPPFLAGS = ${my_CPPFLAGS} -Wall -Woverloaded-virtual
ACLOCAL_AMFLAGS = -I m4

library_includedir=$(includedir)/libdxfrw$(LIBRARY_AGE)
library_include_HEADERS = drw_base.h drw_entities.h drw_interface.h \
	drw_objects.h drw_header.h drw_classes.h libdxfrw.h libdwgr.h
dist_noinst_HEADERS = intern/dxfreader.h intern/dxfwriter.h intern/drw_dbg.h \
	intern/dwgutil.h intern/dwgreader.h intern/dwgreader15.h \
	intern/dwgreader18.h intern/dwgreader21.h intern/dwgreader24.h \
	intern/dwgreader27.h  intern/dwgbuffer.h intern/drw_cptable932.h \
	intern/drw_cptable936.h intern/drw_cptable949.h intern/drw_cptable950.h \
	intern/drw_cptables.h intern/drw_textcodec.h intern/rscodec.h

lib_LTLIBRARIES = libdxfrw.la

libdxfrw_la_SOURCES = drw_entities.cpp drw_objects.cpp drw_header.cpp intern/drw_dbg.cpp \
		      drw_classes.cpp libdwgr.cpp libdxfrw.cpp intern/dwgutil.cpp \
		      intern/dxfreader.cpp intern/dwgreader15.cpp intern/dwgreader18.cpp intern/dwgreader21.cpp \
		      intern/dwgreader24.cpp intern/dwgreader27.cpp intern/dxfwriter.cpp intern/dwgreader.cpp \
		      intern/dwgbuffer.cpp intern/drw_textcodec.cpp intern/rscodec.cpp

libdxfrw_la_LDFLAGS = -no-undefined -version-number $(LIBRARY_AGE):$(LIBRARY_CURRENT):$(LIBRARY_REVISION)

libdxfrw_la_LIBADD =

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = ${top_builddir}/libdxfrw$(LIBRARY_AGE).pc

EXTRA_DIST = ${top_builddir}/autogen.sh ${top_builddir}/makefile.mingw  ${top_builddir}/vs2013/*

libdxfrw.pc: ${top_builddir}/config.status
