# Makefile for compiling EMC task code

include ../Makefile.inc

HEADERS =

SRCS =	\
	emccanon.cc \
	emcsvr.cc \
	emctask.cc \
	emctaskmain.cc \
	inimot.cc \
	minimilltaskintf.cc \
	bridgeporttaskintf.cc \
	emcsh.cc \
	emcprobe.cc \
	keystick.cc

OBJS =	emccanon.o \
	emcsvr.o \
	emctask.o \
	emctaskmain.o \
	inimot.o \
	minimilltaskintf.o \
	bridgeporttaskintf.o \
	emcsh.o \
#	keystick.o


BINS = 	bridgeporttask \
	minimilltask \
	emcsvr \
	inimot \
	emcsh \
	tkemc \
	mini \
	genedit \
	popimage \
	tkbackplot \
	emclog \
	emctesting \
	emccalib \
	emctuning \
	emcdebug \
	genedit \
#	keystick \


LOCAL_CXXFLAGS := $(LOCAL_CFLAGS) $(TCL_CFLAGS)

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) \
	$(BINS:%=$(BIN_DIR)/%) $(RTLIBS:%.o=$(RTLIB_DIR)/%.o)

# Generic rules.
include ../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers

rs274-interp: $(TMP_DIR)/driver.o \
	$(TMP_DIR)/canon_pre.o \
	$(TMP_DIR)/rs274ngc_errors.o \
	$(TMP_DIR)/rs274ngc_pre.o

$(BIN_DIR)/bridgeporttask:	\
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emctask.o \
	$(TMP_DIR)/emccanon.o \
	$(TMP_DIR)/emctaskmain.o \
	$(TMP_DIR)/rs274abc.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/bridgeporttaskintf.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/minimilltask:	\
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emctask.o \
	$(TMP_DIR)/emccanon.o \
	$(TMP_DIR)/emctaskmain.o \
	$(TMP_DIR)/rs274abc.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/minimilltaskintf.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/emcpanel:	\
	$(TMP_DIR)/emcpanel.o \
	$(TMP_DIR)/getinput.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/emcsvr:	\
	$(TMP_DIR)/emcsvr.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/inimot:	\
	$(TMP_DIR)/inimot.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emcglb.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/initraj.o \
	$(TMP_DIR)/iniaxis.o \
	$(TMP_DIR)/minimilltaskintf.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

$(BIN_DIR)/emcdummy:	\
	$(TMP_DIR)/emcdummy.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

# Tcl emcsh

emcsh: $(BIN_DIR)/emcsh

$(BIN_DIR)/emcsh: $(TMP_DIR)/emcsh.o $(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) $(TCL_LIBS) -o $@

$(BIN_DIR)/keystick:	\
	$(TMP_DIR)/keystick.o \
	$(TMP_DIR)/libemc.a
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -lncurses -o $@

# emcprobe

$(TMP_DIR)/emcprobe.o: \
	emcprobe.cc
	( $(COMPILER_SETUP); \
	$(CXX) $< -c $(LOCAL_CFLAGS)  $(LOCAL_CXXFLAGS)\
	-I$(DEVP_INCLUDE_DIR) -I$(RELEASE_INCLUDE_DIR) -I$(RCS_INCLUDE) \
	$(TCL_INC) \
	$(CFLAGS) $(CXXFLAGS) -o $@ )

emcprobe: $(BIN_DIR)/emcprobe
.PHONY: emcprobe

$(BIN_DIR)/emcprobe: \
	$(TMP_DIR)/emcprobe.o \
	$(TMP_DIR)/libemc.a
	$(COMPILER_SETUP); \
	$(CXX) \
	$^ \
	$(RCS_LINK_FLAG) \
	$(TK_LINK) $(TCL_LINK) $(XLINK) \
	$(CXXLINK) -o $@

$(BIN_DIR)/tkemc: \
	tkemc.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/mini: \
	mini.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@


$(BIN_DIR)/tkbackplot: \
	tkbackplot.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/emclog: \
	emclog.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/emctesting: \
	emctesting.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/emccalib: \
	emccalib.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/emctuning: \
	emctuning.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/emcdebug: \
	emcdebug.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/genedit: \
	genedit.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/popimage: \
	popimage.tcl
	-\rm -f $@
	-(cat $<  | sed s/UNKNOWN_PLAT/nonrealtime/ > $@)
	-\chmod a+x $@

$(BIN_DIR)/smini: ../../tcl/smini.tcl
	rm -f $@
	cd $(BIN_DIR) ; \
	ln -fs ../../../tcl/smini.tcl smini

