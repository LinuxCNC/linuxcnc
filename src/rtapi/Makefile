# Makefile for real-time and user-level API

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Public headers (will be copied to INC_DIR)
HEADERS = rtapi.h rtapi_app.h

# Non-Realtime Sources - will be compiled to '*.o'
SRCS = $(RTPREFIX)_ulapi.c

# Realtime Sources - will be compiled to '*.rt.o'
RT_SRCS = $(RTPREFIX)_rtapi.c

# Libraries to be created
LIBS = $(RTLIB_DIR)/$(RTPREFIX)_rtapi.o \
$(LIB_DIR)/$(RTPREFIX)_ulapi.o

# list of all the '*.o' files
OBJS := $(patsubst %.c,%.o,$(SRCS))
# list of all the '*.rt.o' files
RT_OBJS := $(patsubst %.c,rt_%.o,$(RT_SRCS))
# list of all the dependency files (*.d and *.rt.d)
DEPS := $(patsubst %.c,%.d,$(SRCS)) $(patsubst %.c,%.rt.d,$(RT_SRCS))
# list of the public headers in their final location
HDRS := $(patsubst %.h,$(INC_DIR)/%.h,$(HEADERS))

all : headers $(OBJS) $(RT_OBJS) $(LIBS)

# explicit rules to make libraries and binaries

$(RTLIB_DIR)/$(RTPREFIX)_rtapi.o : rt_$(RTPREFIX)_rtapi.o
	- \install -p -m0664 $< $(RTLIB_DIR)/rtapi.o

$(LIB_DIR)/$(RTPREFIX)_ulapi.o : $(RTPREFIX)_ulapi.o
	- \install -p -m0664 $< $(LIB_DIR)/ulapi.o

depend : headers $(DEPS)

clean : clean_up
	- (rm -f $(LIB_DIR)/ulapi.o)
	- (rm -f $(RTLIB_DIR)/rtapi.o)

headers : $(HDRS)

.PHONY: all depend indent install clean headers

# get the dependency files
#-include $(DEPS)

