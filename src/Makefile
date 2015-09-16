# see http://www.cmcrossroads.com/ask-mr-make/6535-tracing-rule-execution-in-gnu-make
# to trace make execution of make in more detail:
#     make VV=1
ifeq ("$(origin VV)", "command line")
    OLD_SHELL := $(SHELL)
    SHELL = $(warning Building $@$(if $<, (from $<))$(if $?, ($? newer)))$(OLD_SHELL)
endif
# Delete the default suffix rules
.SUFFIXES:
.PHONY: default userspace modules clean modclean depclean install python pythonclean cscope cscopeclean

# A "trivial build" is one which should not include dependency information
# either because it should be usable before dependency information can be
# generated or when it is invalid (clean, docclean) or when running as root
# when the user must guarantee in advance that everything is built
# (setuid, install)
ifeq ($(MAKECMDGOALS),)
TRIVIAL_BUILD=no
else
ifeq ($(filter-out clean setuid install tags swish,$(MAKECMDGOALS)),)
TRIVIAL_BUILD=yes
else
TRIVIAL_BUILD=no
endif
endif


# Beautify output
# ---------------------------------------------------------------------------
#
# A simple variant is to prefix commands with $(Q) - that's useful
# for commands that shall be hidden in non-verbose mode.
#
#	$(Q)ln $@ :<
#
# If BUILD_VERBOSE equals 0 then the above command will be hidden.
# If BUILD_VERBOSE equals 1 then the above command is displayed.

ifeq ("$(origin V)", "command line")
  BUILD_VERBOSE = $(V)
endif
ifndef BUILD_VERBOSE
  BUILD_VERBOSE = 0
endif

ifeq ($(BUILD_VERBOSE),1)
  Q =
else
  Q = @
endif

ifeq "$(findstring s,$(MAKEFLAGS))" ""
ECHO=@echo
VECHO=echo
else
ECHO=@true
VECHO=true
endif

ifeq ($(threads),)
# if $(threads) wasn't passed in on the command line, rerun make for
# each flavor/(optional)kernel-source combo
BUILD_ALL_FLAVORS = yes
BUILD_THREAD_MODULES = no
else
# $(threads) was passed in; build modules for this flavor and (if
# applicable) kernel-source
BUILD_ALL_FLAVORS = no
BUILD_THREAD_MODULES = yes
endif # $(threads)

# if KERNELRELEASE is non-empty, we're in kbuild
ifeq ($(KERNELRELEASE),)
BUILD_KBUILD = no
else
BUILD_KBUILD = yes
endif

# extract the version from the kernel source version.h
ifneq ($(KERNELDIR),)
KERNEL_VERS = $(shell $(BASEPWD)/../scripts/kernel-vers.sh $(KERNELDIR))
endif

ifeq ($(BASEPWD),)
BASEPWD := $(shell pwd)
export BASEPWD
include Makefile.inc
ifeq ($(origin PYTHONPATH),undefined)
PYTHONPATH:=$(EMC2_HOME)/lib/python
else
PYTHONPATH:=$(EMC2_HOME)/lib/python:$(PYTHONPATH)
endif
export PYTHONPATH
else
include $(BASEPWD)/Makefile.inc
endif
ifeq ($(BUILD_THREAD_FLAVORS),)
$(error Makefile.inc must specify 'BUILD_THREAD_FLAVORS' and other variables)
endif
# prevent typos
ifeq "$(filter $(threads),$(BUILD_THREAD_FLAVORS))" ""
$(error "$(threads)" is not a valid RTOS flavor; please pick one of:\
    $(BUILD_THREAD_FLAVORS))
endif

# default target:  put before everything else
default:

ifeq ($(BUILD_ALL_FLAVORS),yes)
# Top-level modules target
#
# Re-run 'make modules', once for each configured userland threads
# flavor, and once for each detected kernel for each configured kernel
# threads flavor.  Set 'threads' and where applicable 'KERNELDIR'.
#
# Following tradition, this incarnation of the modules recipe is
# placed far away from all others.
FLAVORUP = $(shell echo $(1) | tr a-z- A-Z_)
FLAVORVAR = $($(call FLAVORUP,$(1))_THREADS_$(2))
modules:  userspace
	@$(foreach f,$(BUILD_THREAD_FLAVORS),\
	    echo "Making modules for flavor $(f)"; \
	    $(if $(findstring kbuild,$(call FLAVORVAR,$(f),BUILD_SYS)),\
		$(foreach kd,$(call FLAVORVAR,$(f),KERNEL_DIRS),\
		    echo "    kernel source dir $(kd)"; \
		    $(MAKE) modules threads=$(f) KERNELDIR=$(kd);\
		    ),\
	        $(MAKE) modules threads=$(f);\
	    )\
	)
ifeq ($(RUN_IN_PLACE)+$(BUILD_DRIVERS),yes+yes)
	@test -f ../libexec/linuxcnc_module_helper -a \
		\( `stat -c %u ../libexec/linuxcnc_module_helper` -ne 0 -o \
		    ! -u ../libexec/linuxcnc_module_helper \) \
	    && need_setuid=1; \
	for f in $(filter-out %-kernel,$(BUILD_THREAD_FLAVORS)); do \
	    test -f ../libexec/rtapi_app_$$f -a \
		    \( 0`stat -c %u ../libexec/rtapi_app_$$f 2>/dev/null` \
			-ne 0 -o ! -u ../libexec/rtapi_app_$$f \) \
		&& need_setuid=1; \
	done; \
	test "$$need_setuid" = 1 && \
	    $(VECHO) -n "You now need to run 'sudo make setuid' " && \
	    $(VECHO) "in order to run in place." || true
endif

endif # BUILD_ALL_FLAVORS


OBJDIR := objects/$(RTDIR_EXT)
DEPDIR := depends/$(RTDIR_EXT)
RTLIBDIR := ../rtlib/$(RTDIR_EXT)

DEP = $(1) -MM -MG -MT "$(2)" $(4) -o $(3).tmp && mv -f "$(3)".tmp "$(3)"

cc-option = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
	     > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

ifeq ($(KERNELRELEASE),)
# When KERNELRELEASE is not defined, this is the userspace build.
# The "modules" target is the gateway to the kernel module build.
default: configs userspace modules

# Print 'entering' all the time
MAKEFLAGS += w

INCLUDE_PREFIX = .

# Create the variables with := so that subsequent += alterations keep it
# as a "substitute at assignment time" variable
TARGETS :=
PYTARGETS :=
# Add to this list directories to be created during 'make install'
INSTALL_DIRS :=

# Submakefiles from each of these directories will be included if they exist
SUBDIRS := \
	\
	machinetalk \
	machinetalk/support \
	machinetalk/messagebus \
	machinetalk/msgcomponents \
	machinetalk/lib \
	machinetalk/config-service \
	machinetalk/webtalk \
	machinetalk/haltalk \
	machinetalk/mkwrapper \
	machinetalk/mklauncher \
	machinetalk/videoserver \
	\
	rtapi/rtapi_math \
	\
	libnml/linklist \
	libnml/cms \
	libnml/rcs \
	libnml/inifile \
	libnml/os_intf \
	libnml/nml \
	libnml/buffer \
	libnml/posemath libnml \
	\
	rtapi/examples/timer \
	rtapi/examples/shmem \
	rtapi/examples \
	rtapi \
	rtapi/shmdrv \
	rtapi/userpci \
	\
	hal/lib \
	hal/i_components \
	hal/components \
	hal/vtable-example \
	hal/userfunct-example \
	hal/drivers \
	hal/user_comps/devices \
	hal/user_comps/mb2hal \
	hal/user_comps \
	hal/user_comps/vismach \
	hal/user_comps/vfs11_vfd \
	hal/user_comps/vfdb_vfd \
	hal/classicladder \
	hal/utils \
	hal \
	hal/support \
	hal/drivers/hal_pru_generic \
	hal/simdrivers \
	hal/cython \
	\
	emc/usr_intf/axis \
	emc/usr_intf/touchy \
	emc/usr_intf/stepconf \
	emc/usr_intf/pncconf \
	emc/usr_intf/gremlin \
	emc/usr_intf/gscreen \
	emc/usr_intf/gmoccapy \
	emc/usr_intf \
	emc/nml_intf \
	emc/task \
	emc/iotask \
	emc/kinematics \
	emc/canterp \
	emc/motion \
	emc/tp \
	emc/ini \
	emc/rs274ngc \
	emc/sai \
	emc \
	emc/pythonplugin \
	\
	module_helper \
	\
	po \


# moved to src/rtapi/Submakefile
# ULAPISRCS := rtapi/$(RTPREFIX)_ulapi.c

# Each item in INCLUDES is transformed into a -I directive later on
# The top directory is always included
INCLUDES := .

USERSRCS :=
PROGRAMS :=

# When used like $(call TOxxx, ...) these turn a list of source files
# into the corresponding list of object files, dependency files,
# or both.  When a source file has to be compiled with special flags,
# TOOBJSDEPS is used.  Confusingly, TOOBJSDEPS includes preprocessed source
# file names, but this is what allows 'make src.i' to produce proper
# preprocessed source when src.c needs a customized compile flag.
# See Submakefile.skel for an example.
TOOBJS = $(patsubst %.cc,objects/%$(2).o,$(patsubst %.c,objects/%$(2).o,$(1)))
TODEPS = $(patsubst %.cc,depends/%$(2).d,$(patsubst %.c,depends/%$(2).d,$(1)))
#-not sure, something fishy here -mah
#TODEPS = $(patsubst %.cc,objects/%.d,$(patsubst %.c,objects/%.d,$(1)))

TOOBJSDEPS = $(call TOOBJS,$(1),$(2)) $(call TODEPS, $(1),$(2))

SUBMAKEFILES := $(patsubst %,%/Submakefile,$(SUBDIRS))
-include $(wildcard $(SUBMAKEFILES))

# This checks that all the things listed in USERSRCS are either C files
# or C++ files
ASSERT_EMPTY = $(if $(1), $(error "Should be empty but is not: $(1)"))
ifdef TARGET_PLATFORM_BEAGLEBONE
# the beaglebone port adds .p (PRU assembly) source files, see hal/components
$(call ASSERT_EMPTY,$(filter-out %.c %.cc %.p %.js  %.ph %.proto, $(USERSRCS)))
else
$(call ASSERT_EMPTY,$(filter-out %.c %.cc %.js %.proto, $(USERSRCS)))
endif
$(call ASSERT_EMPTY,$(filter-out %.c %.proto, $(RTSRCS)))

ifeq ($(BUILD_PYTHON),yes)
$(call TOOBJS,$(PYSRCS)) : EXTRAFLAGS += -fPIC $(call cc-option,-fno-strict-aliasing)
USERSRCS += $(PYSRCS)
endif

# Find the list of object files for each type of source file
CUSERSRCS := $(filter %.c,$(USERSRCS))
CXXUSERSRCS := $(filter %.cc,$(USERSRCS))
CUSEROBJS = $(call TOOBJS,$(CUSERSRCS))
CXXUSEROBJS += $(call TOOBJS,$(CXXUSERSRCS))

# Find the list of build-arch object files for C sources
CUSERSRCS_BUILD := $(filter %.c,$(USERSRCS_BUILD))
CUSEROBJS_BUILD := $(call TOOBJS,$(CUSERSRCS_BUILD),_build)

ifeq ($(TRIVIAL_BUILD),no)

ifeq ($(USE_PROTOBUF),yes)
# force create of %.proto-dependent files and their deps
Makefile: $(GENERATED) $(PROTO_DEPS)
-include $(PROTO_DEPS)
endif

ifdef TARGET_PLATFORM_BEAGLEBONE
ifneq ($(PRU_DEPS),)
Makefile: $(PRU_DEPS)
READ_PRU_DEPS = $(wildcard $(PRU_DEPS))
$(shell echo 1>&2 Reading $(words $(READ_PRU_DEPS))/$(words $(PRU_DEPS)) PRU dependency files)
-include $(wildcard $(READ_PRU_DEPS))
endif
endif

# Find the dependency filenames, then include them all
DEPS := $(sort $(patsubst %.o,%.d,$(CUSEROBJS) $(CXXUSEROBJS) \
	$(CUSEROBJS_BUILD)))
READ_DEPS = $(wildcard $(DEPS))
$(shell echo 1>&2 Reading $(words $(READ_DEPS))/$(words $(DEPS)) dependency files)
-include $(READ_DEPS)
UNREAD_DEPS = $(filter-out $(READ_DEPS), $(DEPS))
LEGACY_DEPS = $(patsubst objects/%, depends/%, $(UNREAD_DEPS))
READ_LEGACY_DEPS = $(wildcard $(LEGACY_DEPS))
ifneq ($(READ_LEGACY_DEPS),)
$(shell echo 1>&2 Reading $(words $(READ_LEGACY_DEPS)) old-style dependency files)
-include $(READ_LEGACY_DEPS)
endif
$(shell echo 1>&2 Done reading dependencies)
endif

# Each directory in $(INCLUDES) is passed as a -I directory when compiling.
INCLUDE := $(patsubst %,-I%, $(INCLUDES))
ifeq ($(BUILD_PYTHON),yes)
INCLUDE += -I$(INCLUDEPY)
endif

# trap build errors by making it possible to compare configure SHA vs build SHA
# see configure.ac GIT_CONFIG_SHA for the configure-time string
GIT_BUILD_SHA := $(shell ../scripts/get-git-sha)

# complain if SHA's dont match
$(info $(shell  ../scripts/check-build-vs-configure-sha))

# Compilation options.	Perhaps some of these should come from Makefile.inc? (CXXFLAGS now does)
INTEGER_OVERFLOW_FLAGS := $(call cc-option,-fwrapv) $(call cc-option, -fno-strict-overflow)
OPT :=  $(INTEGER_OVERFLOW_FLAGS)
DEBUG := -g -Wall -funwind-tables
PROFILE_CFLAGS :=
PROFILE_LDFLAGS :=

CFLAGS := $(INCLUDE) $(OPT) $(DEBUG) $(PROFILE_CFLAGS) -DULAPI $(call cc-option,-std=gnu99 -fgnu89-inline)

# enable runtime comparison of GIT_CONFIG_SHA (config.h) and GIT_BUILD_SHA
CFLAGS += -DGIT_BUILD_SHA=\"'$(GIT_BUILD_SHA)'\"

CXXFLAGS := $(INCLUDE) $(CXXFLAGS) \
	 -DULAPI $(DEBUG) $(OPT) \
	$(PROFILE_CFLAGS) \
	 -DGIT_BUILD_SHA=\"'$(GIT_BUILD_SHA)'\"

ifeq ($(RUN_IN_PLACE),yes)
LDFLAGS := -L$(LIB_DIR) -Wl,-rpath,$(LIB_DIR)
else
LDFLAGS := -Wl,-rpath-link,../lib
endif

# Rules to make .o (object) files
$(sort $(CUSEROBJS)) : objects/%.o: %.c
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@rm -f $@
	$(Q)$(CC) -c $(CFLAGS) $(EXTRAFLAGS) \
		-MP -MD -MF "${@:.o=.d}" -MT "$@" \
		$< -o $@

# Rules to make .o (object) files, build arch
$(sort $(CUSEROBJS_BUILD)) : objects/%_build.o: %.c
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@rm -f $@
	$(Q)$(CC_FOR_BUILD) -c $(CFLAGS) $(EXTRAFLAGS) \
		-MP -MD -MF "${@:.o=.d}" -MT "$@" \
		$< -o $@

$(sort $(CXXUSEROBJS)) : objects/%.o: %.cc
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@rm -f $@
	$(Q)$(CXX) -c $(CXXFLAGS) $(EXTRAFLAGS) \
		-MP -MD -MF "${@:.o=.d}" -MT "$@" \
		$< -o $@

ifeq ($(TRIVIAL_BUILD),no)
configure config.h.in: configure.ac
	@echo "*** configure.ac has changed ***" >&2
	@echo "Not running autoconf to avoid tainting universal build" >&2
	@echo "Please investigate or start from scratch:" >&2
	@echo "./autogen.sh && ./configure --your-configure-args..." >&2
	@exit 1
	# AUTOGEN_TARGET=configure ./autogen.sh

config.status: configure
	if [ -f config.status ]; then ./config.status --recheck; else \
	    echo 1>&2 "*** linuxcnc is not configured.	Run './configure' with appropriate flags."; \
	    exit 1; \
	fi
endif

Makefile: config.h
config.h: config.h.in config.status
	@./config.status -q --header=$@

INFILES = \
	../scripts/linuxcnc \
	../scripts/realtime \
	../scripts/haltcl \
	../scripts/gen-rtapi.ini.sh \
	Makefile.inc \
	Makefile.modinc \
	../tcl/linuxcnc.tcl \
	../scripts/halrun \
	../scripts/rip-environment \
	../scripts/linuxcncmkdesktop \
	../lib/python/nf.py \
	../share/desktop-directories/cnc.directory \
	../share/menus/CNC.menu \
	../share/applications/linuxcnc.desktop \
	../scripts/linuxcnc_var \

$(INFILES): %: %.in config.status
	@./config.status --file=$@

default: $(INFILES)

# For each file to be copied to ../include, its location in the source tree
# is listed here.  Note that due to $(INCLUDE), defined above, the include
# files in the source tree are the ones used when building linuxcnc.  The copy
# in ../include is used when building external components of linuxcnc.
HEADERS := \
    config.h \
    emc/ini/emcIniFile.hh \
    emc/ini/iniaxis.hh \
    emc/ini/initool.hh \
    emc/ini/initraj.hh \
    emc/ini/inihal.hh \
    emc/kinematics/cubic.h \
    emc/kinematics/kinematics.h \
    emc/kinematics/genhexkins.h \
    emc/kinematics/genserkins.h \
    emc/kinematics/pumakins.h \
    emc/tp/tc.h \
    emc/tp/tc_types.h \
    emc/tp/tcq.h \
    emc/tp/tp.h \
    emc/tp/tp_types.h \
    emc/tp/spherical_arc.h \
    emc/tp/blendmath.h \
    emc/tp/tp_shared.h \
    emc/tp/tp_private.h \
    emc/motion/emcmotcfg.h \
    emc/motion/emcmotglb.h \
    emc/motion/motion.h \
    emc/motion/motion_id.h \
    emc/motion/usrmotintf.h \
    emc/motion/state_tag.h \
    emc/nml_intf/canon.hh \
    emc/nml_intf/canon_position.hh \
    emc/nml_intf/emctool.h \
    emc/nml_intf/emc.hh \
    emc/nml_intf/emc_nml.hh \
    emc/nml_intf/emccfg.h \
    emc/nml_intf/emcglb.h \
    emc/nml_intf/emcpos.h \
    emc/nml_intf/emcpose.h \
    emc/nml_intf/interp_return.hh \
    emc/nml_intf/interpl.hh \
    emc/nml_intf/motion_types.h \
    emc/nml_intf/debugflags.h \
    emc/rs274ngc/interp_internal.hh \
    emc/rs274ngc/interp_base.hh \
    emc/rs274ngc/modal_state.hh \
    emc/rs274ngc/rs274ngc.hh \
    hal/lib/config_module.h \
    hal/lib/hal_group.h \
    hal/lib/hal.h \
    hal/lib/hal_internal.h \
    hal/lib/hal_iter.h \
    hal/lib/hal_list.h \
    hal/lib/hal_logging.h \
    hal/lib/hal_parport.h \
    hal/lib/hal_priv.h \
    hal/lib/hal_rcomp.h \
    hal/lib/hal_ring.h \
    hal/lib/vtable.h \
    hal/drivers/hal_gpiomap.h \
    hal/drivers/hal_spi.h \
    libnml/buffer/locmem.hh \
    libnml/buffer/memsem.hh \
    libnml/buffer/phantom.hh \
    libnml/buffer/physmem.hh \
    libnml/buffer/recvn.h \
    libnml/buffer/rem_msg.hh \
    libnml/buffer/sendn.h \
    libnml/buffer/shmem.hh \
    libnml/buffer/tcpmem.hh \
    libnml/cms/cms.hh \
    libnml/cms/cms_aup.hh \
    libnml/cms/cms_cfg.hh \
    libnml/cms/cms_dup.hh \
    libnml/cms/cms_srv.hh \
    libnml/cms/cms_up.hh \
    libnml/cms/cms_user.hh \
    libnml/cms/cms_xup.hh \
    libnml/cms/cmsdiag.hh \
    libnml/cms/tcp_opts.hh \
    libnml/cms/tcp_srv.hh \
    libnml/inifile/inifile.h \
    libnml/inifile/inifile.hh \
    libnml/linklist/linklist.hh \
    libnml/nml/cmd_msg.hh \
    libnml/nml/nml.hh \
    libnml/nml/nml_mod.hh \
    libnml/nml/nml_oi.hh \
    libnml/nml/nml_srv.hh \
    libnml/nml/nml_type.hh \
    libnml/nml/nmldiag.hh \
    libnml/nml/nmlmsg.hh \
    libnml/nml/stat_msg.hh \
    libnml/os_intf/_sem.h \
    libnml/os_intf/sem.hh \
    libnml/os_intf/_shm.h \
    libnml/os_intf/shm.hh \
    libnml/os_intf/_timer.h \
    libnml/os_intf/timer.hh \
    libnml/posemath/posemath.h \
    libnml/posemath/gotypes.h \
    libnml/posemath/gomath.h \
    libnml/posemath/sincos.h \
    libnml/rcs/rcs.hh \
    libnml/rcs/rcs_exit.hh \
    libnml/rcs/rcs_print.hh \
    libnml/rcs/rcsversion.h \
    rtapi/rtapi.h \
    rtapi/rtapi_app.h \
    rtapi/rtapi_bitops.h \
    rtapi/rtapi_byteorder.h \
    rtapi/rtapi_export.h \
    rtapi/rtapi_compat.h \
    rtapi/rtapi_int.h \
    rtapi/rtapi_limits.h \
    rtapi/rtapi_math.h \
    rtapi/rtapi_math64.h \
    rtapi/rtapi_common.h \
    rtapi/rtapi_exception.h \
    rtapi/rtapi_global.h \
    rtapi/rtapi_shmkeys.h \
    rtapi/rtapi_io.h \
    rtapi/rtapi_ctype.h \
    rtapi/rtapi_errno.h \
    rtapi/rtapi_string.h \
    rtapi/rtapi_pci.h \
    rtapi/rtapi_heap.h \
    rtapi/rtapi_heap_private.h \
    rtapi/ring.h \
    rtapi/multiframe.h \
    rtapi/rtapi_mbarrier.h \
    rtapi/$(THREADS_SOURCE).h \
    rtapi/shmdrv/shmdrv.h

# Make each include file a target
TARGETS += $(patsubst %,../include/%,$(foreach h,$(HEADERS),$(notdir $h)))

# Add headers to this list that need to go into subdirectories; their
# respective Submakefiles will need to provide rules to copy them into
# ../install.
TARGETS += $(SUBDIRECTORY_HEADERS)

# Add converting of %.po files
TARGETS += $(patsubst po/%.po, ../share/locale/%/LC_MESSAGES/linuxcnc.mo, $(wildcard po/*.po))
TARGETS += $(patsubst po/gmoccapy/%.po, ../share/locale/%/LC_MESSAGES/gmoccapy.mo, $(wildcard po/gmoccapy/*.po))
TARGETS += $(patsubst po/%.po, objects/%.msg, $(wildcard po/*.po))

# tooledit as standalone app:
TARGETS += ../bin/tooledit
../bin/tooledit: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(TCLSH)")>|$@
	$(Q)(echo "source $(EMC2_TCL_LIB_DIR)/tooledit.tcl")>>$@
	$(Q)(echo "standalone_tooledit")>>$@
	$(Q)chmod +x $@

# ngcgui as standalone app:
TARGETS += ../bin/ngcgui
../bin/ngcgui: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(TCLSH)")>|$@
	$(Q)(echo "source $(EMC2_TCL_LIB_DIR)/ngcgui.tcl")>>$@
	$(Q)(echo "::ngcgui::standalone_ngcgui")>>$@
	$(Q)chmod +x $@

# pyngcgui as standalone app:
TARGETS += ../bin/pyngcgui
../bin/pyngcgui: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(PYTHON)")>|$@
	$(Q)(echo "import pyngcgui")>>$@
	$(Q)(echo "pyngcgui.standalone_pyngcgui()")>>$@
	$(Q)chmod +x $@

# gremlin_view as standalone app:
TARGETS += ../bin/gremlin_view
../bin/gremlin_view: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(PYTHON)")>|$@
	$(Q)(echo "import gremlin_view")>>$@
	$(Q)(echo "gremlin_view.standalone_gremlin_view()")>>$@
	$(Q)chmod +x $@

# halshow as standalone app:
TARGETS += ../bin/halshow
../bin/halshow: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(TCLSH)")>|$@
	$(Q)(echo "source $(EMC2_TCL_LIB_DIR)/bin/halshow.tcl")>>$@
	$(Q)chmod +x $@

# And make userspace depend on $(TARGETS)
userspace: $(TARGETS)

ifeq ($(BUILD_PYTHON),yes)
pythonclean:
	rm -f $(PYTARGETS)
	find ../lib/python -name '*.so' -exec rm {} +
python: $(PYTARGETS)
userspace: python
clean: pythonclean cscopeclean
endif

# This is the gateway into the crazy world of "kbuild", the linux 2.6 system
# for building kernel modules.	Other kernel module build styles need to be
# accomodated here.
#
# (don't build when 'BUILD_ALL_FLAVORS' or 'BUILD_THREADS_MODULES' are set;
# these are intermediate make runs)
ifeq ($(BUILD_SYS)+$(BUILD_THREAD_MODULES),kbuild+yes)
modules:  $(OBJDIR)/.separate-kbuild-artifacts-hack.stamp
	mkdir -p $(RTLIBDIR)
	$(PYTHON) modsilent.py $(MAKE) \
	    -C $(KERNELDIR) \
	    SUBDIRS=$(BASEPWD)/$(OBJDIR) \
	    CC="$(CC)" KBUILD_VERBOSE=$(BUILD_VERBOSE) modules
	cp $(OBJDIR)/Module.symvers $(OBJDIR)/*$(MODULE_EXT) $(RTLIBDIR)
endif  # BUILD_SYS == 'kbuild'

# It looks almost impossible to coerce kbuild into putting build
# artifacts outside the source tree.  The universal build needs to
# keep these artifacts separate for each kthread flavor.  To solve
# this, create a link tree in objects/rtapi-<flavor> with everything
# needed to build kernel modules.
#
# Then in the above 'modules:' target, cd to the link tree and call
# kbuild.
MODULE_SOURCE_DIRS = emc hal libnml rtapi workaround machinetalk
MODULE_SOURCES = Makefile* config* modsilent.py
$(OBJDIR)/.separate-kbuild-artifacts-hack.stamp:
	mkdir -p $(OBJDIR)
	# hard-link contents of $(MODULE_SOURCE_DIRS) to # $(OBJDIR)
	# never follow symbolic links
	cp -rlfd $(MODULE_SOURCE_DIRS) $(OBJDIR)
	cp -lf $(MODULE_SOURCES) $(OBJDIR)
ifeq ($(BUILD_SYS),kbuild)
	# kbuild needs a full copy of this tree with
	cp -rlf halcomp-srcs $(OBJDIR)
endif
	ln -sf $(BASEPWD)/../bin $(OBJDIR)/..
	ln -sf $(BASEPWD)/../scripts $(OBJDIR)/..
	touch $@

# These rules clean things up.	'modclean' cleans files generated by 'modules'
# (except that it doesn't remove the modules that were copied to rtlib)
# 'clean' cleans everything but dependency files, and 'depclean' cleans them
# too.
modclean:
	find -name '.*.cmd' -or -name '*.ko' -or -name '*.mod.c' -or -name '*.mod.o' | xargs rm -f
	-rm -rf .tmp_versions
	-rm -rf halcomp-srcs
	find . -name .tmp_versions |xargs rm -rf
	-rm -f $(RTLIBDIR)/*.ko
	-rm -f $(RTLIBDIR)/*.so

depclean:
	-rm -rf depends

clean: depclean modclean
	find . -name '*.o' |xargs rm -f
	-rm -rf objects
	-rm -f $(TARGETS)
	for flav in $(BUILD_THREAD_FLAVORS); do \
	    rm -rf ../lib/$$flav; \
	done
	-rm -rf ../rtlib ../libexec
	rm -f ../etc/linuxcnc/rtapi.ini
	-rm -f $(COPY_CONFIGS)
	-rm -f $(RTLIBDIR)/*.$(MODULE_EXT)
	-rm -f hal/components/conv_*.comp
	-rm -f hal/components/*_bin.h   # generated by pasm for bb platform
	rm -rf ../share/linuxcnc/stepconf
	-rm -rf ../libexec

# So that nothing is built as root, this rule does not depend on the touched
# files (Note that files in depends/ might be rebuilt, and there's little that
# can be done about it)
fix_perms = test -f $(1) && chown root $(1) && chmod 4750 $(1) || true

ifeq ($(BUILD_DRIVERS),yes)
ifeq ($(HAVE_KERNEL_THREADS),yes)
CHECK_KFLAV_SWITCH=-k
endif
setuid:
	$(call fix_perms,../libexec/linuxcnc_module_helper)
	$(call fix_perms,../libexec/pci_read)
	$(call fix_perms,../libexec/pci_write)
	$(foreach f,$(BUILD_THREAD_FLAVORS),\
	    $(call fix_perms,../libexec/rtapi_app_$(f));)
# check system configuration:  logging, ulimits, udev
	@-../scripts/check-system-configuration.sh $(CHECK_KFLAV_SWITCH)
else
setuid:
	@echo "'make setuid' is not needed if hardware drivers are not used"
endif

# These rules allows a header file from this directory to be installed into
# ../include.  A pair of rules like these will exist in the Submakefile
# of each file that contains headers.
../include/%.h: %.h
	@mkdir -p $(dir $@)
	$(Q)-cp $^ $@
../include/%.hh: %.hh
	@mkdir -p $(dir $@)
	$(Q)-cp $^ $@


DIR=install -d -m 0755 -o root
FILE=install -m 0644 -o root
TREE=cp -dR
CONFIGFILE=install -m 0644
EXE=install -m 0755 -o root
SETUID=install -m 4755 -o root
GLOB=$(wildcard $(1))

ifeq ($(RUN_IN_PLACE),yes)
define ERROR_MESSAGE
You configured run-in-place, but are trying to install.
For an installable version, run configure without --enable-run-in-place
and rebuild
endef
install:
	$(error $(ERROR_MESSAGE))

MENUS = ../share/menus/CNC.menu \
    ../share/desktop-directories/cnc.directory \
    ../share/applications/linuxcnc.desktop \

install-menus install-menu: $(MENUS)
	mkdir -p $(HOME)/.config/menus/applications-merged
	cp $< $(HOME)/.config/menus/applications-merged
else

DOCS_HELP=$(call GLOB,../help/*)
NC_FILES=$(filter-out %/butterfly.ngc,$(call GLOB,../nc_files/*))
TCL=$(call GLOB,../tcl/*.tcl)
TCL_BIN=$(call GLOB,../tcl/bin/*.tcl) ../tcl/bin/popimage

install-test:
	@if type -path dpkg-query > /dev/null 2>&1 ; then  \
		if dpkg-query -S $(DESTDIR)/usr/bin/linuxcnc > /dev/null 2>&1 || dpkg-query -S $(DESTDIR)/usr/bin/emc > /dev/null 2>&1 ; then \
			echo "*** Error: Package version installed in $(DESTDIR)/usr"; \
			echo "Use './configure --enable-run-in-place' or uninstall the linuxcnc package"; \
			echo "before installing."; \
			exit 1; \
		fi \
	fi

install: install-test install-kernel-dep install-kernel-indep
	$(ECHO) "Installed in $(DESTDIR) with prefix $(prefix)"

install-dirs:
	$(DIR)  $(DESTDIR)$(EMC2_RTLIB_DIR) \
		$(DESTDIR)$(sysconfdir)/linuxcnc $(DESTDIR)$(bindir) \
		$(DESTDIR)$(includedir)/linuxcnc \
		$(DESTDIR)$(docdir) $(DESTDIR)$(ncfilesdir) \
		$(DESTDIR)$(EMC2_LIBEXEC_DIR) \
		$(DESTDIR)/etc/X11/app-defaults $(DESTDIR)$(tcldir)/bin \
		$(DESTDIR)$(tcldir)/scripts \
		$(DESTDIR)$(mandir)/man1 \
		$(DESTDIR)$(mandir)/man3 \
		$(DESTDIR)$(mandir)/man9 \
		$(DESTDIR)$(tcldir)/msgs \
		$(DESTDIR)$(localedir)/de/LC_MESSAGES \
		$(DESTDIR)$(datadir)/axis/images \
		$(DESTDIR)$(datadir)/axis/tcl \
		$(DESTDIR)$(datadir)/gscreen/images \
		$(DESTDIR)$(datadir)/gscreen/skins \
		$(DESTDIR)$(datadir)/gmoccapy/images \
		$(DESTDIR)$(datadir)/glade3/catalogs \
		$(DESTDIR)$(datadir)/glade3/pixmaps \
		$(DESTDIR)$(datadir)/gtksourceview-2.0/language-specs \
		$(DESTDIR)$(datadir)/linuxcnc/stepconf \
		$(DESTDIR)$(datadir)/linuxcnc/pncconf/pncconf-help \
		$(DESTDIR)$(sysconfdir)/rsyslog.d \
		$(DESTDIR)$(sysconfdir)/security/limits.d \
		$(DESTDIR)$(sysconfdir)/udev/rules.d \
		$(DESTDIR)$(sampleconfsdir) \
		$(addprefix $(DESTDIR), $(INSTALL_DIRS))
ifeq ($(BUILD_EMCWEB),yes)
	$(DIR)	$(DESTDIR)$(datadir)/linuxcnc/doc-root/css/images \
		$(DESTDIR)$(datadir)/linuxcnc/doc-root/js \
		$(DESTDIR)$(datadir)/linuxcnc/doc-root/res
endif
ifeq ($(USERMODE_PCI),yes)
	$(DIR)	$(DESTDIR)$(includedir)/linuxcnc/userpci
endif

install-kernel-indep: install-dirs
	$(FILE) ../man/man1/*.1 $(DESTDIR)$(mandir)/man1
	$(FILE) ../man/man9/*.9 $(DESTDIR)$(mandir)/man9
	$(FILE) $(filter-out %/skeleton.3hal, $(wildcard ../man/man3/*.3hal)) $(DESTDIR)$(mandir)/man3
	$(FILE) $(filter-out %/skeleton.3rtapi, $(wildcard ../man/man3/*.3rtapi)) $(DESTDIR)$(mandir)/man3
	$(FILE) $(filter-out %/skeleton.9comp, $(wildcard ../man/man9/*.9comp)) $(DESTDIR)$(mandir)/man9
	$(FILE) objects/*.msg $(DESTDIR)$(tcldir)/msgs
	$(EXE) ../scripts/realtime $(DESTDIR)$(bindir)
	$(EXE) ../scripts/halrun $(DESTDIR)$(bindir)
	$(EXE) ../scripts/haltcl $(DESTDIR)$(bindir)
	$(EXE) ../scripts/simulate_probe $(DESTDIR)$(bindir)
	$(EXE) ../scripts/sim_pin $(DESTDIR)$(bindir)
	$(FILE) ../*.png ../*.gif $(DESTDIR)$(datadir)/linuxcnc

	# install all the sample configs, including subdirs (tar is required on debian systems, and common on others)
	((cd ../configs && tar --exclude CVS --exclude .cvsignore --exclude .gitignore -cf - .) | (cd $(DESTDIR)$(sampleconfsdir) && tar -xf -))

	$(EXE) $(filter ../bin/%,$(TARGETS)) $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcnc $(DESTDIR)$(bindir)
	$(EXE) ../scripts/machinekit $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcnc_info $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcnc_var $(DESTDIR)$(bindir)
	$(EXE) ../scripts/latency-test $(DESTDIR)$(bindir)
ifeq ($(HAVE_WORKING_BLT),yes)
	$(EXE) ../scripts/latency-plot $(DESTDIR)$(bindir)
	$(EXE) ../scripts/latency-histogram $(DESTDIR)$(bindir)
endif
	$(EXE) ../scripts/pyvcp_demo $(DESTDIR)$(bindir)
	$(EXE) ../scripts/gladevcp_demo $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcncmkdesktop $(DESTDIR)$(bindir)
	$(FILE) $(filter ../lib/%.a ../lib/%.so.0,$(TARGETS)) $(DESTDIR)$(libdir)
	cp --no-dereference $(filter ../lib/%.so, $(TARGETS)) $(DESTDIR)$(libdir)
	$(FILE) ../rtlib/ulapi-*.so $(DESTDIR)$(EMC2_RTLIB_BASE_DIR)
	-ldconfig $(DESTDIR)$(libdir)
	$(FILE) $(HEADERS) $(DESTDIR)$(includedir)/linuxcnc/
ifeq ($(USERMODE_PCI),yes)
	$(FILE) $(USERPCI_HEADERS) $(DESTDIR)$(includedir)/linuxcnc/userpci
endif
	$(FILE) $(DOCS_HELP) $(DESTDIR)$(docdir)
	$(TREE) $(NC_FILES) $(DESTDIR)$(ncfilesdir)
	$(EXE) ../nc_files/M101 $(DESTDIR)$(ncfilesdir)
	$(FILE) ../tcl/TkLinuxCNC $(DESTDIR)/etc/X11/app-defaults
	$(FILE) ../app-defaults/XEmc $(DESTDIR)/etc/X11/app-defaults
	$(FILE) Makefile.modinc $(DESTDIR)$(datadir)/linuxcnc
	$(FILE) Makefile.inc $(DESTDIR)$(datadir)/linuxcnc
	$(EXE) $(TCL) $(DESTDIR)$(tcldir)
	$(FILE) ../tcl/hal.so $(DESTDIR)$(tcldir)
	$(FILE) ../tcl/linuxcnc.so $(DESTDIR)$(tcldir)
	$(FILE) ../tcl/pkgIndex.tcl $(DESTDIR)$(tcldir)
	$(EXE) $(TCL_BIN) $(DESTDIR)$(tcldir)/bin
	$(FILE) ../tcl/scripts/balloon.tcl ../tcl/scripts/emchelp.tcl $(DESTDIR)$(tcldir)/scripts
	$(EXE) ../tcl/scripts/Set_Coordinates.tcl $(DESTDIR)$(tcldir)/scripts
	$(FILE) ../share/linuxcnc/*.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/stepconf/*.glade $(DESTDIR)$(prefix)/share/linuxcnc/stepconf
	$(FILE) ../share/linuxcnc/touchy.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/gscreen.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/gscreen2.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/gmoccapy/gmoccapy.glade $(DESTDIR)$(prefix)/share/gmoccapy
	$(FILE) ../share/linuxcnc/pncconf.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/gremlin_view.ui $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/popupkeyboard.ui $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../configs/common/linuxcnc.nml $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../src/emc/usr_intf/pncconf/pncconf-help/*.txt $(DESTDIR)$(prefix)/share/linuxcnc/pncconf/pncconf-help
	$(FILE) ../src/emc/usr_intf/pncconf/pncconf-help/*.png $(DESTDIR)$(prefix)/share/linuxcnc/pncconf/pncconf-help

	$(FILE) ../lib/python/gladevcp/hal_python.xml $(DESTDIR)$(datadir)/glade3/catalogs/
	$(FILE) ../lib/python/gladevcp/widget*.png  $(DESTDIR)$(datadir)/glade3/pixmaps/

	$(FILE) ../share/gtksourceview-2.0/language-specs/*.lang  $(DESTDIR)$(datadir)/gtksourceview-2.0/language-specs/
	$(FILE) rtapi/rsyslogd-linuxcnc.conf $(DESTDIR)$(sysconfdir)/rsyslog.d/linuxcnc.conf
	$(FILE) rtapi/shmdrv/limits.d-machinekit.conf \
		$(DESTDIR)$(sysconfdir)/security/limits.d/machinekit.conf
	$(FILE) rtapi/shmdrv/shmdrv.rules \
		$(DESTDIR)$(sysconfdir)/udev/rules.d/50-shmdrv.rules
ifeq ($(BUILD_EMCWEB),yes)
	$(FILE) ../www/css/images/* $(DESTDIR)$(datadir)/linuxcnc/doc-root/css/images
	$(FILE) ../www/css/*css $(DESTDIR)$(datadir)/linuxcnc/doc-root/css
	$(FILE) ../www/js/*js $(DESTDIR)$(datadir)/linuxcnc/doc-root/js
	$(FILE) ../www/res/* $(DESTDIR)$(datadir)/linuxcnc/doc-root/res
	$(FILE) ../www/*html $(DESTDIR)$(datadir)/linuxcnc/doc-root/
endif
	$(FILE) ../etc/linuxcnc/machinekit.ini $(DESTDIR)$(sysconfdir)/linuxcnc
	$(EXE) ../bin/profile_axis $(DESTDIR)$(bindir)

ifeq ($(BUILD_PYTHON),yes)
install-kernel-indep: install-python
install-python: install-dirs
	$(DIR) $(DESTDIR)$(SITEPY) $(DESTDIR)$(SITEPY)/rs274
	$(DIR) $(DESTDIR)$(SITEPY)/touchy
	$(DIR) $(DESTDIR)$(SITEPY)/gscreen
	$(DIR) $(DESTDIR)$(SITEPY)/gmoccapy
	$(DIR) $(DESTDIR)$(SITEPY)/gladevcp
	$(DIR) $(DESTDIR)$(SITEPY)/stepconf
	$(DIR) $(DESTDIR)$(SITEPY)/machinekit
	$(DIR) $(DESTDIR)$(SITEPY)/fdm
	$(DIR) $(DESTDIR)$(datadir)/fdm/thermistor_tables
	$(DIR) $(DESTDIR)$(SITEPY)/drivers
	$(FILE) ../lib/python/*.py ../lib/python/*.so $(DESTDIR)$(SITEPY)
	$(FILE) ../lib/python/rs274/*.py $(DESTDIR)$(SITEPY)/rs274
	$(FILE) ../lib/python/touchy/*.py $(DESTDIR)$(SITEPY)/touchy
	$(FILE) ../lib/python/gscreen/*.py $(DESTDIR)$(SITEPY)/gscreen
	$(FILE) ../lib/python/gmoccapy/*.py $(DESTDIR)$(SITEPY)/gmoccapy
	$(FILE) ../lib/python/gladevcp/*.py $(DESTDIR)$(SITEPY)/gladevcp
	$(FILE) ../lib/python/gladevcp/*.glade $(DESTDIR)$(SITEPY)/gladevcp
	$(FILE) ../lib/python/stepconf/*.py $(DESTDIR)$(SITEPY)/stepconf
	$(FILE) ../lib/python/machinekit/*.py $(DESTDIR)$(SITEPY)/machinekit/
	$(FILE) ../lib/python/machinekit/*.so $(DESTDIR)$(SITEPY)/machinekit/
	$(FILE) ../lib/python/fdm/*.py $(DESTDIR)$(SITEPY)/fdm
	$(FILE) ../lib/python/drivers/*.py $(DESTDIR)$(SITEPY)/drivers
	$(EXE) ../bin/stepconf $(DESTDIR)$(bindir)
	$(EXE) ../bin/pncconf $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_input $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_gpio_mcp23017 $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_pwm_pca9685 $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_storage $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_temp_ads7828 $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_temp_bbb $(DESTDIR)$(bindir)
	$(EXE) ../bin/pyvcp $(DESTDIR)$(bindir)
	$(EXE) ../bin/gladevcp $(DESTDIR)$(bindir)
	$(EXE) ../bin/axis $(DESTDIR)$(bindir)
	$(EXE) ../bin/axis-remote $(DESTDIR)$(bindir)
	$(EXE) ../bin/debuglevel $(DESTDIR)$(bindir)
	$(EXE) ../bin/linuxcnctop $(DESTDIR)$(bindir)
	$(EXE) ../bin/mdi $(DESTDIR)$(bindir)
	$(EXE) ../bin/hal_manualtoolchange $(DESTDIR)$(bindir)
	$(EXE) ../bin/image-to-gcode $(DESTDIR)$(bindir)
	$(EXE) ../bin/touchy $(DESTDIR)$(bindir)
	$(EXE) ../bin/gscreen $(DESTDIR)$(bindir)
	$(EXE) ../bin/gmoccapy $(DESTDIR)$(bindir)
	$(EXE) $(patsubst %.py,../bin/%,$(VISMACH_PY)) $(DESTDIR)$(bindir)
	$(FILE) ../share/linuxcnc/machinekit-wizard.gif $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) emc/usr_intf/axis/etc/axis_light_background $(DESTDIR)$(docdir)
	$(FILE) emc/usr_intf/axis/README $(DESTDIR)$(docdir)/README.axis
	$(FILE) ../share/axis/images/*.png ../share/axis/images/*.gif ../share/axis/images/*.xbm ../share/axis/images/*.ngc $(DESTDIR)$(datadir)/axis/images
	$(FILE) ../share/axis/tcl/*.tcl $(DESTDIR)$(datadir)/axis/tcl
	$(FILE) ../share/gscreen/images/*.gif $(DESTDIR)$(datadir)/gscreen/images
	$(TREE) ../share/gscreen/skins/* $(DESTDIR)$(datadir)/gscreen/skins
	$(FILE) ../share/gmoccapy/images/*.png ../share/gmoccapy/images/*.gif ../share/gmoccapy/images/*.svg $(DESTDIR)$(datadir)/gmoccapy/images
	$(FILE) ../share/fdm/thermistor_tables/*.txt $(DESTDIR)$(datadir)/fdm/thermistor_tables
	$(EXE) ../bin/configserver $(DESTDIR)$(bindir)
	$(EXE) ../bin/videoserver $(DESTDIR)$(bindir)
	$(EXE) ../bin/mkwrapper $(DESTDIR)$(bindir)
	$(EXE) ../bin/mklauncher $(DESTDIR)$(bindir)
	$(EXE) ../bin/gremlin $(DESTDIR)$(bindir)
	$(EXE) ../bin/lintini $(DESTDIR)$(bindir)
	$(EXE) ../bin/teach-in $(DESTDIR)$(bindir)
	$(EXE) ../bin/tracking-test $(DESTDIR)$(bindir)
	$(EXE) ../bin/yapps $(DESTDIR)$(bindir)
	$(EXE) ../scripts/gcode-to-ngc $(DESTDIR)$(bindir)
	$(EXE) ../scripts/g1-to-g23 $(DESTDIR)$(bindir)
endif

install-kernel-dep: install-dirs
	$(SETUID) ../libexec/rtapi_app_* $(DESTDIR)$(EMC2_LIBEXEC_DIR)
	$(SETUID) ../libexec/linuxcnc_module_helper $(DESTDIR)$(EMC2_LIBEXEC_DIR)
ifeq ($(BUILD_DRIVERS),yes)
	$(SETUID) ../libexec/pci_write $(DESTDIR)$(EMC2_LIBEXEC_DIR)
	$(SETUID) ../libexec/pci_read $(DESTDIR)$(EMC2_LIBEXEC_DIR)
endif
	$(EXE) ../libexec/inivar $(DESTDIR)$(EMC2_LIBEXEC_DIR)
	$(EXE) ../libexec/rtapi_msgd $(DESTDIR)$(EMC2_LIBEXEC_DIR)
	$(EXE) ../libexec/flavor $(DESTDIR)$(EMC2_LIBEXEC_DIR)
	$(FILE) ../etc/linuxcnc/rtapi.ini $(DESTDIR)$(sysconfdir)/linuxcnc

	# RTAPI modules:  install userland flavor .so modules into
	# e.g. /usr/lib/linuxcnc/modules and kthread flavor .ko
	# modules for each kver into e.g. /lib/modules/<kver>/linuxcnc
	for flavor in $(BUILD_THREAD_FLAVORS); do \
	    if test $${flavor%-kernel} = $${flavor}; then \
		echo Installing modules for userland flavor $$flavor; \
		$(DIR) $(DESTDIR)$(EMC2_RTLIB_BASE_DIR)/$$flavor; \
		$(FILE) ../rtlib/$$flavor/* \
		    $(DESTDIR)$(EMC2_RTLIB_BASE_DIR)/$$flavor; \
	    else \
		echo Installing modules for kthreads flavor $$flavor; \
		for srcdir in ../rtlib/$$flavor/*; do \
		    kver=`basename $$srcdir`; \
		    echo Installing modules for flavor $$flavor, kver $$kver; \
		    $(DIR) $(DESTDIR)/lib/modules/$$kver/linuxcnc; \
		    $(FILE) $$srcdir/* $(DESTDIR)/lib/modules/$$kver/linuxcnc; \
		done; \
	    fi; \
	done
ifeq ($(USE_SHMDRV),yes)
	for srcdir in ../rtlib/shmdrv/*; do \
	    kver=`basename $$srcdir`; \
	    echo Installing shmdrv.ko module for kernel version $$kver; \
	    $(DIR) $(DESTDIR)/lib/modules/$$kver/linuxcnc; \
	    $(FILE) $$srcdir/* $(DESTDIR)/lib/modules/$$kver/linuxcnc; \
	done;
endif
endif # ! RUN_IN_PLACE

CONF=../configs
COMMON=$(CONF)/common
CONFILES=$(addsuffix /$(1), $(filter-out $(COMMON) $(CONF),\
                                         ${shell find ${CONF} -type d -print}))

.PHONY: configs
COPY_CONFIGS = $(patsubst %,../configs/%/core_stepper.hal, \
                sim \
                by_interface/parport/stepper \
                by_machine/sherline/Sherline3Axis \
                by_machine/sherline/SherlineLathe \
                by_machine/cooltool \
                by_interface/parport/classicladder/demo_step_cl)

COPY_CONFIGS += $(patsubst %,../configs/%/core_servo.hal, \
                sim \
                by_interface/vitalsystems \
                by_interface/servotogo \
                by_interface/vigilant)

COPY_CONFIGS += $(patsubst %,../configs/%/core_sim.hal, \
                sim)

COPY_CONFIGS += $(patsubst %,../configs/%/core_sim9.hal, \
                sim)

COPY_CONFIGS += $(patsubst %,../configs/%/axis_manualtoolchange.hal, \
                sim \
                by_interface/pluto/lathe-pluto\
                by_interface/parport/plasma-thc-sim)

show_configs_copy_stuff:
	@echo CONFILES=$(CONFILES)
	@echo CALL:$(call CONFILES,axis_manualtoolchange.hal)
	@echo CALL:$(call CONFILES,core_stepper.hal)
	@echo CALL:$(call CONFILES,core_servo.hal)
	@echo CALL:$(call CONFILES,core_sim.hal)
	@echo CALL:$(call CONFILES,core_sim9.hal)
	@echo COPY_CONFIGS=$(COPY_CONFIGS)

configs: $(COPY_CONFIGS)

$(call CONFILES,axis_manualtoolchange.hal): %/axis_manualtoolchange.hal: ../configs/common/axis_manualtoolchange.hal
	$(Q)-cp $< $@
$(call CONFILES,core_stepper.hal): %/core_stepper.hal: ../configs/common/core_stepper.hal
	$(Q)-cp $< $@
$(call CONFILES,core_servo.hal): %/core_servo.hal: ../configs/common/core_servo.hal
	$(Q)-cp $< $@
$(call CONFILES,core_sim.hal): %/core_sim.hal: ../configs/common/core_sim.hal
	$(Q)-cp $< $@
$(call CONFILES,core_sim9.hal): %/core_sim9.hal: ../configs/common/core_sim9.hal
	$(Q)-cp $< $@

endif # userspace

ifneq ($(KERNELRELEASE),)
# KERNELRELEASE is non-empty, therefore we are building modules using
# the "kbuild" system.
#
INCLUDE_PREFIX = $(BASEPWD)


# When the Submakefile is included in the kbuild Makefile, the
# module's obj-m and foo-objs definitions may remain in the Submakefile
include $(BASEPWD)/hal/lib/Submakefile
include $(BASEPWD)/machinetalk/Submakefile
include $(BASEPWD)/hal/i_components/Submakefile
include $(BASEPWD)/hal/components/Submakefile
include $(BASEPWD)/hal/vtable-example/Submakefile
include $(BASEPWD)/hal/userfunct-example/Submakefile
include $(BASEPWD)/machinetalk/msgcomponents/Submakefile
include $(BASEPWD)/rtapi/Submakefile
include $(BASEPWD)/rtapi/shmdrv/Submakefile
include $(BASEPWD)/rtapi/rtapi_math/Submakefile
endif # KERNELRELEASE non-empty

# $(BASEPWD) is used here, instead of relative paths, because that's
# what kbuild seems to require

EXTRA_CFLAGS = $(RTFLAGS) \
	-D__MODULE__ \
	-I$(INCLUDE_PREFIX) \
	-I$(INCLUDE_PREFIX)/libnml/linklist \
	-I$(INCLUDE_PREFIX)/libnml/cms \
	-I$(INCLUDE_PREFIX)/libnml/rcs \
	-I$(INCLUDE_PREFIX)/libnml/inifile \
	-I$(INCLUDE_PREFIX)/libnml/os_intf \
	-I$(INCLUDE_PREFIX)/libnml/nml \
	-I$(INCLUDE_PREFIX)/libnml/buffer \
	-I$(INCLUDE_PREFIX)/libnml/posemath \
	-I$(INCLUDE_PREFIX)/protobuf/nanopb \
	-I$(INCLUDE_PREFIX)/rtapi \
	-I$(INCLUDE_PREFIX)/hal/lib \
	-I$(INCLUDE_PREFIX)/emc/nml_intf \
	-I$(INCLUDE_PREFIX)/emc/kinematics \
	-I$(INCLUDE_PREFIX)/emc/motion \
	-I$(INCLUDE_PREFIX)/emc/tp \
	-I$(INCLUDE_PREFIX)/machinetalk/nanopb \
	-DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT \
	-DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER \
	$(call cc-option,-mieee-fp) \
	$(KERNEL_MATH_CFLAGS)

ifdef TARGET_PLATFORM_BEAGLEBONE
EXTRA_CFLAGS += -I$(INCLUDE_PREFIX)/hal/support/pru
endif
ifeq ($(BUILD_SYS),kbuild)
EXTRA_CFLAGS += $(call cc-option,-Wframe-larger-than=2560) $(call cc-option,-Wno-declaration-after-statement)
endif



# For each module, there's an addition to obj-m or obj-$(CONFIG_foo)
# plus a definition of foo-objs, which contains the full path to the
# object file(s) that the module contains.  Unfortunately, this setup pollutes
# the source directory with object files and other temporaries, but I can't
# find a way around it.

# These lists would be best moved to their respective subdirectories'
# Submakefiles to make this more modular and avoid maintaining
# multiple lists of the same things.  This won't work as-is right now
# because the Submakefiles are included in a conditional block that is
# not evaluated during kernel module build.

# Subdirectory:  rtapi

# obj-$(CONFIG_RTAPI) += rtapi.o

# # Sources for all thread systems
# rtapi-objs := \
# 	rtapi/instance.o \
# 	rtapi/rtapi_support.o \
# 	rtapi/rtapi_common.o \
# 	rtapi/rtapi_task.o \
# 	rtapi/rtapi_shmem.o \
# 	rtapi/rtapi_ring.o \
# 	rtapi/rtapi_time.o \
# 	rtapi/rtapi_io.o \
# 	rtapi/$(THREADS_SOURCE).o
# # Kernel threads need rtapi_module.c
# ifeq ($(BUILD_SYS),kbuild)
# rtapi-objs += rtapi/rtapi_module.o
# endif
# # User threads need rtapi_pci.c
# ifeq ($(BUILD_SYS),user-dso)
# rtapi-objs += rtapi/rtapi_pci.o
# rtapi-objs += rtapi/shmdrv/shmdrvapi.o
# # and rtapi_app_main/exit stubs
# rtapi-objs += rtapi/rtapi_main.o
# # and autodetect/compat checks
# rtapi-objs += rtapi/rtapi_compat.o
# endif


# Subdirectory: rtapi/examples (unneeded?)

# Subdirectory: hal/components
obj-$(CONFIG_BOSS_PLC) += boss_plc.o
boss_plc-objs := hal/components/boss_plc.o $(MATHSTUB)
obj-$(CONFIG_ENCODER) += encoder.o
encoder-objs := hal/components/encoder.o $(MATHSTUB)
obj-$(CONFIG_COUNTER) += counter.o
counter-objs := hal/components/counter.o $(MATHSTUB)
obj-$(CONFIG_ENCODER_RATIO) += encoder_ratio.o
encoder_ratio-objs := hal/components/encoder_ratio.o $(MATHSTUB)
obj-$(CONFIG_STEPGEN) += stepgen.o
stepgen-objs := hal/components/stepgen.o $(MATHSTUB)
obj-$(CONFIG_LCD) += lcd.o
lcd-objs := hal/components/lcd.o $(MATHSTUB)
obj-$(CONFIG_MATRIX_KB) += matrix_kb.o
matrix_kb-objs := hal/components/matrix_kb.o $(MATHSTUB)
obj-$(CONFIG_MUX_GENERIC) += mux_generic.o
mux_generic-objs := hal/components/mux_generic.o $(MATHSTUB)
obj-$(CONFIG_PWMGEN) += pwmgen.o
pwmgen-objs := hal/components/pwmgen.o $(MATHSTUB)
obj-$(CONFIG_SIGGEN) += siggen.o
siggen-objs := hal/components/siggen.o $(MATHSTUB)
obj-$(CONFIG_PID) += threads.o
threads-objs := hal/components/threads.o $(MATHSTUB)
obj-$(CONFIG_SUPPLY) += supply.o
supply-objs := hal/components/supply.o $(MATHSTUB)
obj-$(CONFIG_SIM_ENCODER) += sim_encoder.o
sim_encoder-objs := hal/components/sim_encoder.o $(MATHSTUB)
obj-$(CONFIG_WATCHDOG) += watchdog.o
watchdog-objs := hal/components/watchdog.o $(MATHSTUB)
obj-$(CONFIG_MODMATH) += modmath.o
modmath-objs := hal/components/modmath.o $(MATHSTUB)
obj-$(CONFIG_STREAMER) += streamer.o
streamer-objs := hal/components/streamer.o $(MATHSTUB)
obj-$(CONFIG_SAMPLER) += sampler.o
sampler-objs := hal/components/sampler.o $(MATHSTUB)

# Subdirectory: hal/support
ifdef TARGET_PLATFORM_BEAGLEBONE
ifeq ($(BUILD_SYS),user-dso)
obj-m += hal_pru.o
hal_pru-objs := hal/components/hal_pru.o hal/support/pru/prussdrv.o $(LIBPTHREAD)
obj-m += hal_prudebug.o
hal_prudebug-objs := hal/components/hal_prudebug.o hal/support/pru/prussdrv.o $(LIBPTHREAD)
endif
endif

# Subdirectory: hal/drivers
ifeq ($(BUILD_DRIVERS),yes)
#ifeq ($(BUILD_SYS),kbuild)
obj-$(CONFIG_HAL_PARPORT) += hal_parport.o
hal_parport-objs := hal/drivers/hal_parport.o $(MATHSTUB)
obj-$(CONFIG_PCI_8255) += pci_8255.o
pci_8255-objs := hal/drivers/pci_8255.o
obj-$(CONFIG_HAL_TIRO) += hal_tiro.o
hal_tiro-objs := hal/drivers/hal_tiro.o $(MATHSTUB)
obj-$(CONFIG_HAL_STG) += hal_stg.o
hal_stg-objs := hal/drivers/hal_stg.o $(MATHSTUB)
obj-$(CONFIG_HAL_VTI) += hal_vti.o
hal_vti-objs := hal/drivers/hal_vti.o $(MATHSTUB)
#obj-$(CONFIG_HAL_EVOREG) += hal_evoreg.o
#hal_evoreg-objs := hal/drivers/hal_evoreg.o $(MATHSTUB)
obj-$(CONFIG_HAL_MOTENC) += hal_motenc.o
hal_motenc-objs := hal/drivers/hal_motenc.o $(MATHSTUB)
obj-$(CONFIG_HAL_AX521H) += hal_ax5214h.o
hal_ax5214h-objs := hal/drivers/hal_ax5214h.o $(MATHSTUB)
obj-$(CONFIG_HAL_SPEAKER) += hal_speaker.o
hal_speaker-objs := hal/drivers/hal_speaker.o $(MATHSTUB)
obj-$(CONFIG_HAL_SKELETON) += hal_skeleton.o
hal_skeleton-objs := hal/drivers/hal_skeleton.o $(MATHSTUB)

ifdef TARGET_PLATFORM_RASPBERRY
obj-$(CONFIG_HAL_GPIO) += hal_gpio.o
hal_gpio-objs := hal/drivers/hal_gpio.o hal/drivers/cpuinfo.o
obj-$(CONFIG_HAL_SPI) += hal_spi.o
hal_spi-objs := hal/drivers/hal_spi.o
endif

ifdef TARGET_PLATFORM_BEAGLEBONE
ifeq ($(BUILD_SYS),user-dso)
obj-$(CONFIG_HAL_GPIO) += hal_bb_gpio.o
hal_bb_gpio-objs := hal/drivers/hal_bb_gpio.o
obj-$(CONFIG_PEPPER) += pepper.o
pepper-objs := hal/components/pepper.o $(MATHSTUB)
endif

ifdef TARGET_PLATFORM_ZEDBOARD
obj-$(CONFIG_HAL_GPIO) += hal_zed_gpio.o
hal_zed_gpio-objs := hal/drivers/hal_zed_gpio.o
obj-$(CONFIG_HAL_CAN) += hal_zed_can.o
hal_zed_can-objs := hal/drivers/hal_zed_can.o
endif

obj-m += hal_arm335xQEP.o
hal_arm335xQEP-objs := hal/drivers/hal_arm335xQEP.o $(MATHSTUB)

endif

ifeq ($(BUILD_SYS),user-dso)
obj-m += gpiomap.o
gpiomap-objs := hal/drivers/hal_gpiomap.o
obj-m += gpiouse.o
gpiouse-objs := hal/drivers/hal_gpiouse.o
endif

# hal_pru_generic driver
ifdef TARGET_PLATFORM_BEAGLEBONE
ifeq ($(BUILD_SYS),user-dso)
obj-m += hal_pru_generic.o
hal_pru_generic-objs :=                           \
    hal/drivers/hal_pru_generic/hal_pru_generic.o \
    hal/drivers/hal_pru_generic/pwmgen.o          \
    hal/drivers/hal_pru_generic/stepgen.o         \
    hal/drivers/hal_pru_generic/encoder.o         \
    hal/support/pru/prussdrv.o                    \
    $(LIBPTHREAD)
endif
endif

# these won't compile as-is with userland threading

ifneq ($(BUILD_SYS),user-dso)
obj-$(CONFIG_HAL_M5I20) += hal_m5i20.o
hal_m5i20-objs := hal/drivers/hal_m5i20.o $(MATHSTUB)
obj-$(CONFIG_HAL_PPMC) += hal_ppmc.o
hal_ppmc-objs := hal/drivers/hal_ppmc.o $(MATHSTUB)
obj-$(CONFIG_OPTO_AC5) += opto_ac5.o
opto_ac5-objs := hal/drivers/opto_ac5.o $(MATHSTUB)
obj-$(CONFIG_HAL_GM) += hal_gm.o
hal_gm-objs := hal/drivers/hal_gm.o $(MATHSTUB)
endif

# Gnu make 'expression' syntax is pathetic. What were these guys smoking?
# turn on by default
BUILD_HOSTMOT2=yes
ifeq ($(USERMODE_PCI),yes)
# reconfirm our intent..
BUILD_HOSTMOT2=yes
else
#except if userland and USERMODE_PCI isnt set
ifeq ($(BUILD_SYS),user-dso)
BUILD_HOSTMOT2=no
endif
endif

ifeq ($(BUILD_HOSTMOT2),yes)
obj-$(CONFIG_HOSTMOT2) += hostmot2.o hm2_7i43.o hm2_7i90.o hm2_pci.o hm2_test.o
ifeq ($(BUILD_SYS),user-dso)
obj-$(CONFIG_HOSTMOT2) += hm2_eth.o
hm2_eth-objs  :=			  \
    hal/drivers/mesa-hostmot2/hm2_eth.o   \
    $(MATHSTUB)
endif
#obj-$(CONFIG_HOSTMOT2) += hostmot2.o hm2_pci.o hm2_7i43.o
hostmot2-objs +=			  \
    hal/drivers/mesa-hostmot2/hostmot2.o  \
    hal/drivers/mesa-hostmot2/backported-strings.o  \
    hal/drivers/mesa-hostmot2/ioport.o	  \
    hal/drivers/mesa-hostmot2/encoder.o   \
    hal/drivers/mesa-hostmot2/abs_encoder.o\
	hal/drivers/mesa-hostmot2/resolver.o  \
    hal/drivers/mesa-hostmot2/pwmgen.o	  \
    hal/drivers/mesa-hostmot2/tp_pwmgen.o \
	hal/drivers/mesa-hostmot2/sserial.o   \
    hal/drivers/mesa-hostmot2/stepgen.o   \
	hal/drivers/mesa-hostmot2/bspi.o	  \
	hal/drivers/mesa-hostmot2/uart.o	  \
    hal/drivers/mesa-hostmot2/watchdog.o  \
    hal/drivers/mesa-hostmot2/pins.o	  \
    hal/drivers/mesa-hostmot2/dpll.o  \
    hal/drivers/mesa-hostmot2/led.o	  \
    hal/drivers/mesa-hostmot2/tram.o	  \
    hal/drivers/mesa-hostmot2/raw.o	  \
    hal/drivers/mesa-hostmot2/bitfile.o   \
    $(MATHSTUB)
hm2_7i90-objs :=			  \
    hal/drivers/mesa-hostmot2/hm2_7i90.o  \
    hal/drivers/mesa-hostmot2/bitfile.o   \
    $(MATHSTUB)
hm2_7i43-objs :=			  \
    hal/drivers/mesa-hostmot2/hm2_7i43.o  \
    hal/drivers/mesa-hostmot2/bitfile.o   \
    $(MATHSTUB)
hm2_pci-objs  :=			  \
    hal/drivers/mesa-hostmot2/hm2_pci.o   \
    hal/drivers/mesa-hostmot2/bitfile.o   \
    $(MATHSTUB)
hm2_test-objs :=			  \
    hal/drivers/mesa-hostmot2/hm2_test.o  \
    hal/drivers/mesa-hostmot2/bitfile.o   \
    $(MATHSTUB)
setsserial-objs :=			  \
    hal/drivers/mesa-hostmot2/setsserial.o  \
    $(MATHSTUB)
endif

ifeq ($(BUILD_HOSTMOT2),yes)
obj-$(CONFIG_HOSTMOT2) += hm2_pci.o
hm2_pci-objs  :=			  \
    hal/drivers/mesa-hostmot2/hm2_pci.o   \
    hal/drivers/mesa-hostmot2/bitfile.o   \
    $(MATHSTUB)
endif

obj-$(CONFIG_PROBE_PARPORT) += probe_parport.o
probe_parport-objs := hal/drivers/probe_parport.o $(MATHSTUB)
endif

obj-$(CONFIG_CLASSICLADDER_RT) += classicladder_rt.o
classicladder_rt-objs := hal/classicladder/module_hal.o $(MATHSTUB)
classicladder_rt-objs += hal/classicladder/arithm_eval.o
classicladder_rt-objs += hal/classicladder/arrays.o
classicladder_rt-objs += hal/classicladder/calc.o
classicladder_rt-objs += hal/classicladder/calc_sequential.o
classicladder_rt-objs += hal/classicladder/manager.o
classicladder_rt-objs += hal/classicladder/symbols.o
classicladder_rt-objs += hal/classicladder/vars_access.o

ifdef SEQUENTIAL_SUPPORT
classicladder_rt-objs += hal/classicladder/calc_sequential_rt.o
endif

obj-m += scope_rt.o
scope_rt-objs := hal/utils/scope_rt.o $(MATHSTUB)


obj-m += trivkins.o
trivkins-objs := emc/kinematics/trivkins.o

obj-m += 5axiskins.o
5axiskins-objs := emc/kinematics/5axiskins.o

obj-m += maxkins.o
maxkins-objs := emc/kinematics/maxkins.o

obj-m += gantrykins.o
gantrykins-objs := emc/kinematics/gantrykins.o

obj-m += rotatekins.o
rotatekins-objs := emc/kinematics/rotatekins.o

obj-m += tripodkins.o
tripodkins-objs := emc/kinematics/tripodkins.o

obj-m += lineardeltakins.o
lineardeltakins-objs := emc/kinematics/lineardeltakins.o

obj-m += genhexkins.o
genhexkins-objs := emc/kinematics/genhexkins.o
genhexkins-objs += libnml/posemath/_posemath.o
genhexkins-objs += libnml/posemath/sincos.o $(MATHSTUB)

obj-m += genserkins.o
genserkins-objs := emc/kinematics/genserkins.o
genserkins-objs += libnml/posemath/gomath.o
genserkins-objs += libnml/posemath/sincos.o $(MATHSTUB)

obj-m += pumakins.o
pumakins-objs := emc/kinematics/pumakins.o
pumakins-objs += libnml/posemath/_posemath.o
pumakins-objs += libnml/posemath/sincos.o $(MATHSTUB)

obj-m += scarakins.o
scarakins-objs := emc/kinematics/scarakins.o
scarakins-objs += libnml/posemath/_posemath.o
scarakins-objs += libnml/posemath/sincos.o $(MATHSTUB)

obj-$(CONFIG_MOTMOD) += motmod.o
motmod-objs := emc/kinematics/cubic.o
motmod-objs += emc/motion/motion.o
motmod-objs += emc/motion/command.o
motmod-objs += emc/motion/control.o
motmod-objs += emc/motion/homing.o
motmod-objs += emc/motion/emcmotglb.o
motmod-objs += emc/motion/emcmotutil.o
motmod-objs += emc/motion/stashf.o
motmod-objs += emc/motion/dbuf.o
motmod-objs += libnml/posemath/_posemath.o
motmod-objs += libnml/posemath/sincos.o $(MATHSTUB)

# obj-m += vtexport.o
# vtexport-objs := hal/vtable-example/vtexport.o
# vtexport-objs += hal/vtable-example/vcode.o

obj-m += icomp.o
icomp-objs := hal/icomp-example/icomp.o

# clashes with component in i_components
#obj-m += lutn.o
#lutn-objs := hal/icomp-example/lutn.o

obj-m += ufdemo.o
ufdemo-objs := hal/userfunct-example/ufdemo.o

obj-m += tp.o
tp-objs := $(addprefix emc/tp/, \
	tc.o 		\
	tcq.o 		\
	tp.o 		\
	tpmain.o 	\
	blendmath.o 	\
	spherical_arc.o 	\
	) 		\
	emc/nml_intf/emcpose.o \
	libnml/posemath/_posemath.o \
	libnml/posemath/sincos.o $(MATHSTUB)


TORTOBJS = $(foreach file,$($(patsubst %.o,%,$(1))-objs), $(OBJDIR)/$(file))


# USER_DSO module building
# (don't build when 'BUILD_ALL_FLAVORS' or 'BUILD_THREADS_MODULES' are set;
# these are intermediate make runs)
ifeq ($(BUILD_SYS)+$(BUILD_THREAD_MODULES),user-dso+yes)
EXTRA_CFLAGS += -fPIC

RTOBJS := $(sort $(foreach mod,$(obj-m),$(call TORTOBJS,$(mod))))

RTDEPS := $(sort $(patsubst $(OBJDIR)/%.o,$(DEPDIR)/%.d, $(RTOBJS)))
IS_POWERPC = test `uname -m` = ppc -o `uname -m` = ppc64
modules: $(patsubst %.o,$(RTLIBDIR)/%.so,$(obj-m))
$(RTLIBDIR)/%.so:
	$(ECHO) Linking $@
	@mkdir -p $(dir $@)
	$(Q)$(LD) -r -o $(OBJDIR)/$*.tmp \
	    $^
	@if ! $(IS_POWERPC); then $(OBJCOPY) -j .rtapi_export -O binary \
	    $(OBJDIR)/$*.tmp $(OBJDIR)/$*.exp; fi
	@if ! $(IS_POWERPC); then $(OBJCOPY) \
	    -G __x86.get_pc_thunk.bx -G __i686.get_pc_thunk.bx \
	    `xargs -r0n1 echo -G < $(OBJDIR)/$*.exp | grep -ve '^-G $$' | \
	        sort -u` $(OBJDIR)/$*.tmp; fi
	$(Q)$(CC) -shared -Bsymbolic $(LDFLAGS) -o $@ $(OBJDIR)/$*.tmp \
	$(EXTRA_LDFLAGS)

$(sort $(RTDEPS)): $(DEPDIR)/%.d: %.c
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	$(Q)$(call DEP,$(CC),$@ \
		$(patsubst depends/%.d,objects/%.o,$@),$@,$(OPT) \
		$(DEBUG) $(EXTRA_CFLAGS) $<)

# Rules to make .o (object) files
$(sort $(RTOBJS)) : $(OBJDIR)/%.o : %.c $(DEPDIR)/%.d
	$(ECHO) Compiling realtime $<
	@rm -f $@
	@mkdir -p $(dir $@)
	$(Q)$(CC) -c $(OPT) $(DEBUG) $(EXTRA_CFLAGS) $< -o $@

endif # end BUILD_SYS=user-dso

# # ------ not used anymore -------------
# ifeq ($(BUILD_SYS),normal)
# modules: $(patsubst %,$(RTLIBDIR)/%,$(obj-m))
# RTOBJS := $(sort $(foreach mod,$(obj-m),$(call TORTOBJS,$(mod))))
# RTDEPS := $(sort $(patsubst objects/%.o,depends/%.d, $(RTOBJS)))

# $(sort $(RTDEPS)): $(DEPDIR)/%.d: %.c
# 	@mkdir -p $(dir $@)
# 	$(ECHO) Depending $<
# 	$(Q)$(call DEP,$(CC),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(EXTRA_CFLAGS) $<)


# # Rules to make .o (object) files
# $(sort $(RTOBJS)) : $(OBJDIR)/%.o : %.c
# 	$(ECHO) Compiling realtime $<
# 	@mkdir -p $(dir $@)
# 	$(Q)$(CC) -c -DRTAPI -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(KERNELDIR)/include $(EXTRA_CFLAGS) $< -o $@

# $(RTLIBDIR)/%.o:
# 	$(ECHO) Linking $@
# 	$(Q)ld -r -static -S  $(LDFLAGS) -o $@ $^ $(EXTRALINK)
# endif # end 'normal'

# build kernel RTAPI modules
ifneq "$(filter normal user-dso,$(BUILD_SYS))" ""
#$(RTLIBDIR)/instance$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(instance-objs))
#$(RTLIBDIR)/rtapi$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(rtapi-objs))
$(RTLIBDIR)/classicladder_rt$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(classicladder_rt-objs))
$(RTLIBDIR)/boss_plc$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(boss_plc-objs))
$(RTLIBDIR)/encoder$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(encoder-objs))
$(RTLIBDIR)/counter$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(counter-objs))
$(RTLIBDIR)/encoder_ratio$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(encoder_ratio-objs))
$(RTLIBDIR)/stepgen$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(stepgen-objs))
$(RTLIBDIR)/lcd$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(lcd-objs))
$(RTLIBDIR)/matrix_kb$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(matrix_kb-objs))
$(RTLIBDIR)/mux_generic$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(mux_generic-objs))
$(RTLIBDIR)/freqgen$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(freqgen-objs))
$(RTLIBDIR)/pwmgen$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(pwmgen-objs))
$(RTLIBDIR)/siggen$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(siggen-objs))
$(RTLIBDIR)/at_pid$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(at_pid-objs))
$(RTLIBDIR)/threads$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(threads-objs))
$(RTLIBDIR)/supply$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(supply-objs))
$(RTLIBDIR)/sim_encoder$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(sim_encoder-objs))
$(RTLIBDIR)/watchdog$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(watchdog-objs))
$(RTLIBDIR)/modmath$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(modmath-objs))
$(RTLIBDIR)/streamer$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(streamer-objs))
$(RTLIBDIR)/sampler$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(sampler-objs))
$(RTLIBDIR)/hal_parport$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_parport-objs))
$(RTLIBDIR)/probe_parport$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(probe_parport-objs))


ifeq ($(BUILD_SYS),user-dso)
$(RTLIBDIR)/probe_parport$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(probe_parport-objs))
$(RTLIBDIR)/gpiomap$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(gpiomap-objs))
$(RTLIBDIR)/gpiouse$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(gpiouse-objs))
endif

ifeq ($(USERMODE_PCI),yes)
$(RTLIBDIR)/hostmot2$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hostmot2-objs))
$(RTLIBDIR)/hm2_7i43$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hm2_7i43-objs))
$(RTLIBDIR)/hm2_7i90$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hm2_7i90-objs))
$(RTLIBDIR)/hm2_pci$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hm2_pci-objs))
ifeq ($(BUILD_SYS),user-dso)
$(RTLIBDIR)/hm2_eth$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hm2_eth-objs))
endif
$(RTLIBDIR)/hm2_test$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hm2_test-objs))
endif

ifdef TARGET_PLATFORM_BEAGLEBONE
ifeq ($(BUILD_SYS),user-dso)
$(RTLIBDIR)/hal_pru$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_pru-objs))
$(RTLIBDIR)/hal_pru_generic$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_pru_generic-objs))
$(RTLIBDIR)/hal_prudebug$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_prudebug-objs))
$(RTLIBDIR)/hal_gpio$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_gpio-objs))
$(RTLIBDIR)/hal_bb_gpio$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_bb_gpio-objs))
$(RTLIBDIR)/pepper$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(pepper-objs))
$(RTLIBDIR)/hal_arm335xQEP$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_arm335xQEP-objs))
endif
endif


ifdef TARGET_PLATFORM_RASPBERRY
$(RTLIBDIR)/hal_gpio$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_gpio-objs))
$(RTLIBDIR)/hal_spi$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_spi-objs))
endif

ifdef TARGET_PLATFORM_ZEDBOARD
$(RTLIBDIR)/hal_zed_gpio$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_zed_gpio-objs))
$(RTLIBDIR)/hal_zed_can$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_zed_can-objs))
endif

$(RTLIBDIR)/pci_8255$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(pci_8255-objs))
$(RTLIBDIR)/hal_tiro$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_tiro-objs))
$(RTLIBDIR)/hal_stg$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_stg-objs))
$(RTLIBDIR)/hal_vti$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_vti-objs))
#$(RTLIBDIR)/hal_evoreg$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_evoreg-objs))
$(RTLIBDIR)/hal_motenc$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_motenc-objs))
$(RTLIBDIR)/hal_ax5214h$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_ax5214h-objs))
$(RTLIBDIR)/hal_ppmc$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_ppmc-objs))
$(RTLIBDIR)/hal_skeleton$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_skeleton-objs))
$(RTLIBDIR)/hal_speaker$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_speaker-objs))
$(RTLIBDIR)/opto_ac5$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(opto_ac5-objs))
$(RTLIBDIR)/scope_rt$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(scope_rt-objs))
$(RTLIBDIR)/motmod$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(motmod-objs))
$(RTLIBDIR)/trivkins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(trivkins-objs))
$(RTLIBDIR)/5axiskins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(5axiskins-objs))
$(RTLIBDIR)/maxkins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(maxkins-objs))
$(RTLIBDIR)/gantrykins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(gantrykins-objs))
$(RTLIBDIR)/rotatekins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(rotatekins-objs))
$(RTLIBDIR)/tripodkins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(tripodkins-objs))
$(RTLIBDIR)/lineardeltakins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(lineardeltakins-objs))
$(RTLIBDIR)/genhexkins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(genhexkins-objs))
$(RTLIBDIR)/genserkins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(genserkins-objs))
$(RTLIBDIR)/pumakins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(pumakins-objs))
$(RTLIBDIR)/scarakins$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(scarakins-objs))
$(RTLIBDIR)/hal_gm$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_gm-objs))

$(RTLIBDIR)/tp$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(tp-objs))
$(RTLIBDIR)/ufdemo$(MODULE_EXT):	$(addprefix $(OBJDIR)/,$(ufdemo-objs))
$(RTLIBDIR)/icomp$(MODULE_EXT):	$(addprefix $(OBJDIR)/,$(icomp-objs))
$(RTLIBDIR)/lutn$(MODULE_EXT):	$(addprefix $(OBJDIR)/,$(lutn-objs))

ifeq ($(TRIVIAL_BUILD),no)
READ_RTDEPS = $(wildcard $(RTDEPS))
$(shell echo 1>&2 Reading $(words $(READ_RTDEPS))/$(words $(RTDEPS)) realtime dependency files)
-include $(READ_RTDEPS)
$(shell echo 1>&2 Done reading realtime dependencies)
endif
endif  # build kernel RTAPI modules

# Phony so that it is always rebuilt when requested, not because it
# shouldn't exist as a file
.PHONY: tags

# dont create tags from these directories
NOTAGSIN := \
	depends \
	objects \
	machinetalk/generated \
	machinetalk/nanopb

CTAGOPTS := $(addprefix --exclude=,$(NOTAGSIN))
ETAGOPTS := $(CTAGOPTS)
FINDOPTS :=  -not \( $(patsubst %,-path ./% -prune -o,$(NOTAGSIN)) -type d \)

tags:
	ctags-exuberant --extra=+fq \
		$(CTAGOPTS) \
		'--langmap=make:+(Submakefile),make:+(Makefile.inc),c:+.comp' \
		-I EXPORT_SYMBOL+,RTAPI_MP_INT+,RTAPI_MP_LONG+,RTAPI_MP_STRING+ \
		-I RTAPI_MP_ARRAY_INT+,RTAPI_MP_ARRAY_LONG+,RTAPI_MP_ARRAY_STRING+ \
		-I MODULE_AUTHOR+,MODULE_DESCRIPTION+,MODULE_LICENSE+ \
		-R . ../tcl ../scripts ../share/axis/tcl
	rm -f TAGS
	find . -type f -name '*.[ch]' $(FINDOPTS) | xargs  etags -l c --append
	find . -type f -name '*.cc'  $(FINDOPTS) | xargs  etags -l c++ --append
	find . -type f -name '*.hh'  $(FINDOPTS) | xargs  etags -l c++ --append
	find . -type f -name '*.proto'  $(FINDOPTS) | xargs  etags -l c++ --append

# etags from exuberant-ctags package
etags:
	etags --extra=+fq \
		$(ETAGOPTS) \
		'--langmap=make:+(Submakefile),make:+(Makefile.inc),c:+.comp' \
		-I EXPORT_SYMBOL+,RTAPI_MP_INT+,RTAPI_MP_LONG+,RTAPI_MP_STRING+ \
		-I RTAPI_MP_ARRAY_INT+,RTAPI_MP_ARRAY_LONG+,RTAPI_MP_ARRAY_STRING+ \
		-I MODULE_AUTHOR+,MODULE_DESCRIPTION+,MODULE_LICENSE+ \
		-R . ../tcl ../scripts ../share/axis/tcl
	find . -type f -name '*.[ch]' $(FINDOPTS) |xargs etags --language-force=C --append
	find . -type f -name '*.cc'  $(FINDOPTS) |xargs etags --language-force=C++ --append
	find . -type f -name '*.hh'  $(FINDOPTS) |xargs etags --language-force=C++ --append
	find . -type f -name '*.proto'  $(FINDOPTS) |xargs etags --language-force=C++ --append

# emacs etags; does not support tcl
eetags:
	@rm -f TAGS
	@find .  $(FINDOPTS) -type f -name '*.[ch]'  |xargs etags --language=c   --append
	@find .  $(FINDOPTS) -type f -name '*.cc'    |xargs etags --language=c++ --append
	@find .  $(FINDOPTS) -type f -name '*.hh'    |xargs etags --language=c++ --append
	@find .  $(FINDOPTS) -type f -name '*.proto' |xargs etags --language=c++ --append

.PHONY: swish
swish:
	swish-e -c .swish_config -v 0 -i $(BASEPWD) \
		$(dir $(BASEPWD))tcl \
		$(dir $(BASEPWD))share/axis/tcl \
		$(dir $(BASEPWD))lib/python \
		$(dir $(BASEPWD))scripts \
		$(dir $(BASEPWD))configs \
		$(dir $(BASEPWD))man/man1

# When you depend on objects/var-ZZZ you are depending on the contents of the
# variable ZZZ, which is assumed to depend on a Makefile, a Submakefile, or
# Makefile.inc
objects/var-%: Makefile $(wildcard $(SUBMAKEFILES)) Makefile.inc
	@mkdir -p $(dir $@)
	@echo $($*) > $@.tmp
	@sh move-if-change $@.tmp $@

../lib/%.so: ../lib/%.so.0
	ln -sf $(notdir $<) $@

cscope:
	cscope -Rb

cscope/man/:
	bash -c 'for f in `find ./ -name "cscope.*out"`;do rm $$f;done'

NOSETESTS := $(wildcard ../nosetests/*.py)
nosetest:
	$(foreach var,$(NOSETESTS),nosetests -v $(var);)
