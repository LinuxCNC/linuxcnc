# Makefile for compiling EMC IO controllers

include ../../Makefile.inc

HEADERS = emcio.hh \
	usrmotintf.h


SRCS =  bridgeportaux.cc \
	bridgeportcool.cc \
	bridgeportiomain.cc \
	bridgeportlube.cc \
	bridgeportspin.cc \
	bridgeporttool.cc \
	dumbio.cc \
	extbridgeportio.cc \
	extminimillio.cc \
	iosh.cc \
	minimillaux.cc \
	minimilliomain.cc \
	minimilltool.cc \
	usrmotintf.c \
	usrmot.c

OBJS =  bridgeportaux.o \
	bridgeportcool.o \
	bridgeportiomain.o \
	bridgeportlube.o \
	bridgeportspin.o \
	bridgeporttool.o \
	dumbio.o \
	extbridgeportio.o \
	extminimillio.o \
	iosh.o \
	minimillaux.o \
	minimilliomain.o \
	minimilltool.o \
	usrmotintf.o \
	usrmot.o

BINS =  bridgeportio \
	dumbio \
	minimillio \
	iosh \
	usrmot

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) \
	$(BINS:%=$(BIN_DIR)/%) $(RTLIBS:%.o=$(RTLIB_DIR)/%.o)

LOCAL_CXXFLAGS := $(LOCAL_CFLAGS) $(TCL_CFLAGS)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers


# bridgeportio for Bridgeport-class machines uses parallel port digital IO,
# null stubbed analog IO
$(BIN_DIR)/bridgeportio: \
	$(TMP_DIR)/bridgeportiomain.o \
	$(TMP_DIR)/bridgeportaux.o \
	$(TMP_DIR)/bridgeportcool.o \
	$(TMP_DIR)/bridgeportlube.o \
	$(TMP_DIR)/bridgeportspin.o \
	$(TMP_DIR)/bridgeporttool.o \
	$(TMP_DIR)/extsimaio.o \
	$(TMP_DIR)/simaio.o \
	$(TMP_DIR)/extppt.o \
	$(TMP_DIR)/parport.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

# simio uses fully simulated digital IO with Bridgeport logic
$(BIN_DIR)/simio:	\
	$(TMP_DIR)/bridgeportiomain.o \
	$(TMP_DIR)/bridgeportaux.o \
	$(TMP_DIR)/bridgeportcool.o \
	$(TMP_DIR)/bridgeportlube.o \
	$(TMP_DIR)/bridgeportspin.o \
	$(TMP_DIR)/bridgeporttool.o \
	$(TMP_DIR)/extsimdio.o \
	$(TMP_DIR)/extsimaio.o \
	$(TMP_DIR)/simdio.o \
	$(TMP_DIR)/simaio.o \
	$(TMP_DIR)/simbase.o \
	$(TMP_DIR)/parport.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

# minimillio for NIST minimill uses fully simulated IO, but no spindle
$(BIN_DIR)/minimillio:	\
	$(TMP_DIR)/minimilliomain.o \
	$(TMP_DIR)/minimillaux.o \
	$(TMP_DIR)/minimilltool.o \
	$(TMP_DIR)/extminimillio.o \
	$(TMP_DIR)/simdio.o \
	$(TMP_DIR)/simaio.o \
	$(TMP_DIR)/simbase.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/iosh: \
	$(TMP_DIR)/iosh.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) $(TCL_LIBS) -o $@

$(BIN_DIR)/usrmot:	\
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/usrmot.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/pid.o \
	$(TMP_DIR)/getinput.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/tkio: \
	tkio.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/dumbio: \
	$(TMP_DIR)/dumbio.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@
