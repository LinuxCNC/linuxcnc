# EMC motion control code

include ../../Makefile.inc

HEADERS =cubic.h \
	emcmot.h \
	emcmotcfg.h \
	emcmotglb.h \
	emcmotlog.h \
	kinematics.h \
	mmxavg.h \
	pid.h \
	tc.h \
	tp.h \
	emcprocfs.h \
	emcioctl.h

SRCS =

OBJS =  pid.o \
	emcmotutil.o \
	emcmotlog.o \
	emcmotglb.o

ifeq ($(KERNELRELEASE),2.4)

RT_SRCS = cubic.c \
	emcmot.c \
	emcmotglb.c \
	emcmotlog.c \
	emcmotutil.c \
	mmxavg.c \
	pid.c \
	tc.c \
	tp.c \
	trivkins.c \
	\
	emcmodule.c \
	emcstepper.c \
	emccommand.c \
	emcprocfs.c \
	test_module.c

RT_OBJS = cubic.o \
	emcmot.o \
	emcmotglb.o \
	trivkins.o \
	mmxavg.o \
	emcmotlog.o \
	emcmotutil.o \
	tc.o \
	tp.o \
	pid.o \
	\
	emcmodule.o\
	emcstepper.o \
	emccommand.o \
	emcprocfs.o

RTLIBS = freqmod.o \
	smdromod.o \
	vitalmod.o \
	test_module.o

endif

BINS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) \
	$(RTLIBS:%.o=$(RTLIB_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

kclean:
	- rm -f .*.cmd *.o* *.mod.* *.ko
	- rm -fR .tmp_versions 


.PHONY: all clean install indent depend headers

$(BIN_DIR)/usrmot:	\
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/usrmot.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/pid.o \
	$(TMP_DIR)/getinput.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o
	$(CXX) $^ -L$(LIB_DIR) -lnml -Wl,-rpath,$(LIB_DIR) -o $@

# frequency stepping and other types
$(RTLIB_DIR)/freqmod.o: \
	$(RTTMP_DIR)/emcmot.o \
	$(RTTMP_DIR)/emcmotglb.o \
	$(RTTMP_DIR)/tp.o \
	$(RTTMP_DIR)/tc.o \
	$(RTTMP_DIR)/trivkins.o \
	$(RTTMP_DIR)/cubic.o \
	$(RTTMP_DIR)/pid.o \
	$(RTTMP_DIR)/extsmmot.o \
	$(RTTMP_DIR)/extppt.o \
	$(RTTMP_DIR)/parport.o \
	$(RTTMP_DIR)/extsimaio.o \
	$(RTTMP_DIR)/simaio.o \
	$(RTTMP_DIR)/mmxavg.o \
	$(RTTMP_DIR)/emcmotlog.o \
	$(RTTMP_DIR)/emcmotutil.o \
	\
	$(RTTMP_DIR)/emcmodule.o \
	$(RTTMP_DIR)/emcstepper.o \
	$(RTTMP_DIR)/emccommand.o \
	$(RTTMP_DIR)/emcprocfs.o
	ld -r -static $^ $(RTLIB_DIR)/libpm.a -o $@

# frequency stepping with DRO board
$(RTLIB_DIR)/smdromod.o: \
	$(RTTMP_DIR)/emcmot.o \
	$(RTTMP_DIR)/emcmotglb.o \
	$(RTTMP_DIR)/tp.o \
	$(RTTMP_DIR)/tc.o \
	$(RTTMP_DIR)/trivkins.o \
	$(RTTMP_DIR)/cubic.o \
	$(RTTMP_DIR)/pid.o \
	$(RTTMP_DIR)/extsmdromot.o \
	$(RTTMP_DIR)/dro.o \
	$(RTTMP_DIR)/parport.o \
	$(RTTMP_DIR)/extsimdio.o \
	$(RTTMP_DIR)/extsimaio.o \
	$(RTTMP_DIR)/simdio.o \
	$(RTTMP_DIR)/simaio.o \
	$(RTTMP_DIR)/mmxavg.o \
	$(RTTMP_DIR)/emcmotlog.o \
	$(RTTMP_DIR)/emcmotutil.o \
	\
	$(RTTMP_DIR)/emcmodule.o \
	$(RTTMP_DIR)/emcstepper.o \
	$(RTTMP_DIR)/emccommand.o \
	$(RTTMP_DIR)/emcprocfs.o
	ld -r -static $^ $(RTLIB_DIR)/libpm.a -o $@

$(RTLIB_DIR)/vitalmod.o: \
	$(RTTMP_DIR)/vital.o \
	$(RTTMP_DIR)/extvitalmot.o \
	$(RTTMP_DIR)/emcmot.o \
	$(RTTMP_DIR)/emcmotglb.o \
	$(RTTMP_DIR)/tp.o \
	$(RTTMP_DIR)/tc.o \
	$(RTTMP_DIR)/trivkins.o \
	$(RTTMP_DIR)/cubic.o \
	$(RTTMP_DIR)/pid.o \
	$(RTTMP_DIR)/mmxavg.o \
	$(RTTMP_DIR)/emcmotlog.o \
	$(RTTMP_DIR)/emcmotutil.o \
	\
	$(RTTMP_DIR)/emcmodule.o \
	$(RTTMP_DIR)/emccommand.o \
	$(RTTMP_DIR)/emcprocfs.o
	ld -r -static $^ $(RTLIB_DIR)/libpm.a -o $@
