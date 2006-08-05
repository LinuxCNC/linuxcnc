# Delete the default suffix rules
.SUFFIXES:
.PHONY: default userspace modules clean modclean depclean install axis

ifeq "$(findstring s,$(MAKEFLAGS))" ""
ECHO=@echo
VECHO=echo
else
ECHO=@true
VECHO=true
endif

ifeq ($(BASEPWD),)
BASEPWD := $(shell pwd)
export BASEPWD
endif
include $(BASEPWD)/Makefile.inc

ifeq ($(DEPSTYLE),gcc3)
DEP = "$(1)" -MM -MG -MT "$(2)" $(4) -o $(3).tmp && mv -f "$(3)".tmp "$(3)"
else
DEP = bash depend.sh "$(1)" "$(2)" "$(3)" $(4)
endif

cc-option = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
             > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

ifeq ($(KERNELRELEASE),)
# When KERNELRELEASE is not defined, this is the userspace build.
# The "modules" target is the gateway to the kernel module build.
default: configs userspace modules
ifeq ($(RUN_IN_PLACE),yes)
	@if ! [ `stat -c %u ../bin/emc_module_helper` -eq 0 -a -u ../bin/emc_module_helper ]; then $(VECHO) "You now need to run 'sudo make setuid' in order to run in place."; fi
endif

ifeq ($(BUILD_AXIS),yes)
default: axis
axis: userspace
	$(ECHO) 'make: Entering directory `$(AXIS_DIR)'\'
	@cd $(AXIS_DIR) && env EMCROOT=$(EMC2_HOME) python setup.py -q install
	$(ECHO) 'make: Leaving directory `$(AXIS_DIR)'\'
clean: axisclean
axisclean:
	rm -rf $(AXIS_DIR)/build
endif

# Create the variable with := so that subsequent += alterations keep it
# as a "substitute at assignment time" variable
TARGETS :=

# Submakefiles from each of these directories will be included if they exist
SUBDIRS := \
    libnml/linklist libnml/cms libnml/rcs libnml/inifile libnml/os_intf \
    libnml/nml libnml/buffer libnml/posemath libnml \
    \
    rtapi/examples/timer rtapi/examples/semaphore rtapi/examples/shmem \
    rtapi/examples/extint rtapi/examples/fifo rtapi/examples rtapi \
    \
    hal/components hal/drivers hal/user_comps/vcp hal/user_comps/devices \
    hal/user_comps hal/classicladder hal/utils hal \
    \
    emc/usr_intf emc/nml_intf emc/task emc/iotask emc/kinematics emc/canterp \
    emc/motion emc/ini emc/rs274ngc emc \
    \
    module_helper \
    \
    po

ULAPISRCS := rtapi/$(RTPREFIX)_ulapi.c 

# Each item in INCLUDES is transformed into a -I directive later on
# The top directory is always included
INCLUDES := .

USERSRCS := 
PROGRAMS := 

# When used like $(call TOxxx, ...) these turn a list of source files
# into the corresponding list of object files, dependency files,
# or both.  When a source file has to be compiled with special flags,
# TOOBJSDEPS is used.  See Submakefile.skel for an example.
TOOBJS = $(patsubst %.cc,objects/%.o,$(patsubst %.c,objects/%.o,$(1)))
TODEPS = $(patsubst %.cc,depends/%.d,$(patsubst %.c,depends/%.d,$(1)))
TOOBJSDEPS = $(call TOOBJS,$(1)) $(call TODEPS,$(1))

ifneq ($(MAKECMDGOALS),clean)
SUBMAKEFILES := $(patsubst %,%/Submakefile,$(SUBDIRS))
-include $(SUBMAKEFILES)
endif

# This checks that all the things listed in USERSRCS are either C files
# or C++ files
ASSERT_EMPTY = $(if $(1), $(error "Should be empty but is not: $(1)"))
$(call ASSERT_EMPTY,$(filter-out %.c %.cc, $(USERSRCS)))
$(call ASSERT_EMPTY,$(filter-out %.c, $(RTSRCS)))

# Find the list of object files for each type of source file
CUSEROBJS := $(call TOOBJS,$(filter %.c,$(USERSRCS)))
CXXUSEROBJS += $(call TOOBJS,$(filter %.cc,$(USERSRCS)))

ifneq ($(MAKECMDGOALS),clean)
# Find the dependency filenames, then include them all
DEPS := $(sort $(patsubst objects/%.o,depends/%.d,$(CUSEROBJS) $(CXXUSEROBJS)))
-include $(DEPS)
Makefile: $(DEPS)
endif

# Each directory in $(INCLDUES) is passed as a -I directory when compiling.
INCLUDE := $(patsubst %,-I%, $(INCLUDES)) -I$(RTDIR)/include 

# Compilation options.  Perhaps some of these should come from Makefile.inc?
OPT := -O
DEBUG := -g -Wall
CFLAGS := $(INCLUDE) $(OPT) $(DEBUG) -DULAPI $(call cc-option, -Wdeclaration-after-statement)
CXXFLAGS := $(INCLUDE) $(OPT) $(DEBUG) -DULAPI

ifeq ($(RUN_IN_PLACE),yes)
# Used when linking for rip only so that the .so files are found.
# important not to use this for installed version, since we want
# those libs to come from the system path.
LDFLAGS := -Wl,-rpath,$(LIB_DIR)
endif

# Rules to make .d (dependency) files.
$(sort $(call TODEPS, $(filter %.cc,$(USERSRCS)))): depends/%.d: %.cc
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CXX),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(CXXFLAGS) $(EXTRAFLAGS) $<)

$(sort $(call TODEPS, $(filter %.c,$(USERSRCS)))): depends/%.d: %.c 
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CC),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(CFLAGS) $(EXTRAFLAGS) $<)

# Rules to make .o (object) files
$(sort $(CUSEROBJS)) : objects/%.o: %.c
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@$(CC) -c $(CFLAGS) $(EXTRAFLAGS) $< -o $@

$(sort $(CXXUSEROBJS)) : objects/%.o: %.cc
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@$(CXX) -c $(CXXFLAGS) $(EXTRAFLAGS) $< -o $@

config.h: config.h.in config.status
	@./config.status -q --header=$@

%: %.in config.status
	@./config.status --file=$@

default: \
	../docs/man/man1/emc.1 ../scripts/emc ../scripts/realtime \
	../scripts/rtapi.conf Makefile.inc depend.sh Makefile.modinc \
	../tcl/emc.tcl 

# For each file to be copied to ../include, its location in the source tree
# is listed here.  Note that due to $(INCLUDE), defined above, the include
# files in the source tree are the ones used when building emc2.  The copy
# in ../include is used when building external components of emc2.
HEADERS := \
    config.h \
    emc/ini/iniaxis.hh \
    emc/ini/initool.hh \
    emc/ini/initraj.hh \
    emc/kinematics/cubic.h \
    emc/kinematics/kinematics.h \
    emc/kinematics/genhexkins.h \
    emc/kinematics/mmxavg.h \
    emc/kinematics/tc.h \
    emc/kinematics/tp.h \
    emc/motion/emcmotcfg.h \
    emc/motion/emcmotglb.h \
    emc/motion/motion.h \
    emc/motion/usrmotintf.h \
    emc/nml_intf/canon.hh \
    emc/nml_intf/emc.hh \
    emc/nml_intf/emccfg.h \
    emc/nml_intf/emcglb.h \
    emc/nml_intf/emcpos.h \
    emc/nml_intf/interp.hh \
    emc/nml_intf/interp_return.hh \
    emc/nml_intf/interpl.hh \
    emc/rs274ngc/interp_internal.hh \
    emc/rs274ngc/rs274ngc.hh \
    hal/hal.h \
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
    libnml/inifile/inifile.hh \
    libnml/linklist/linklist.hh \
    libnml/nml/cmd_msg.hh \
    libnml/nml/nml.hh \
    libnml/nml/nml_mod.hh \
    libnml/nml/nml_oi.hh \
    libnml/nml/nml_srv.hh \
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
    libnml/posemath/sincos.h \
    libnml/rcs/rcs.hh \
    libnml/rcs/rcs_exit.hh \
    libnml/rcs/rcs_print.hh \
    libnml/rcs/rcsversion.h \
    rtapi/procfs_macros.h \
    rtapi/rtapi.h \
    rtapi/rtapi_app.h \
    rtapi/rtapi_math.h \
    rtapi/rtapi_math_i386.h \

# Make each include file a target
TARGETS += $(patsubst %,../include/%,$(foreach h,$(HEADERS),$(notdir $h)))

# Add converting of %.po files
TARGETS += $(patsubst %.po, %.mo, $(wildcard po/*.po))

# And make userspace depend on $(TARGETS)
userspace: $(TARGETS)

# This is the gateway into the crazy world of "kbuild", the linux 2.6 system
# for building kernel modules.  Other kernel module build styles need to be
# accomodated here.
ifeq ($(BUILD_SYS),kbuild)

ifndef KDIR
ifeq ($(wildcard /lib/modules/$(shell uname -r)/build/Makefile),)
$(error KDIR is not set and /lib/modules/$(shell uname -r)/build is not a valid kernel build location.)
endif
KDIR := $(shell cd /lib/modules/$(shell uname -r)/build && /bin/pwd)
endif

# '-o $(KDIR)/Module.symvers' silences warnings about that file being missing
modules:
	$(MAKE) -C $(KDIR) SUBDIRS=`pwd` CC=$(CC) V=0 -o $(KDIR)/Module.symvers modules 
	-cp *$(MODULE_EXT) ../rtlib/
endif

# These rules clean things up.  'modclean' cleans files generated by 'modules'
# (except that it doesn't remove the modules that were copied to rtlib)
# 'clean' cleans everything but dependency files, and 'depclean' cleans them
# too.
modclean:
	find -name '.*.cmd' -or -name '*.ko' -or -name '*.mod.c' -or -name '*.mod.o' | xargs rm -f
	-rm -rf .tmp_versions
	find . -name .tmp_versions |xargs rm -rf
	-rm -f ../rtlib/*.ko

depclean:
	-rm -rf depends

clean: depclean modclean
	find . -name '*.o' |xargs rm -f
	-rm -rf objects 
	-rm -f $(TARGETS)
	-rm -f $(COPY_CONFIGS)

# So that nothing is built as root, this rule does not depend on the touched
# files (Note that files in depends/ might be rebuilt, and there's little that
# can be done about it)
setuid:
	chown root ../bin/emc_module_helper
	chmod 4750 ../bin/emc_module_helper

# These rules allows a header file from this directory to be installed into
# ../include.  A pair of rules like these will exist in the Submakefile
# of each file that contains headers.
../include/%.h: %.h
	-cp $^ $@
../include/%.hh: %.hh
	-cp $^ $@

DIR=install -d -m 0755 -o root
FILE=install -m 0644 -o root
CONFIGFILE=install -m 0644
EXE=install -m 0755 -o root
SETUID=install -m 4755 -o root
NOCVS=$(filter-out %CVS,$(1))
GLOB=$(call NOCVS,$(wildcard $(1)))
CONFIGS=$(subst ../configs/,,$(call GLOB,../configs/*))

ifeq ($(RUN_IN_PLACE),yes)
define ERROR_MESSAGE
You configured run-in-place, but are trying to install.  
For an installable version, run configure without --enable-run-in-place 
and rebuild
endef
install:
	$(error $(ERROR_MESSAGE))
else

define INSTALL_CONFIG
$(DIR) $(DESTDIR)$(sysconfdir)/emc2/sample-configs/$(1)
$(FILE) $(call GLOB,../configs/$(1)/*) $(DESTDIR)$(sysconfdir)/emc2/sample-configs/$(1)

endef
INSTALL_CONFIGS=$(foreach config,$(CONFIGS), $(call INSTALL_CONFIG,$(config)))

DOCS=AUTHORS Hal_Introduction.pdf INSTALL NEWS README

DOCS_HELP=$(call GLOB,../docs/help/*)
NC_FILES=$(call GLOB,../nc_files/*)
TCL=$(call GLOB,../tcl/*.tcl)
TCL_BIN=$(call GLOB,../tcl/bin/*.tcl) ../tcl/bin/popimage

install: install-kernel-dep install-kernel-indep
	$(ECHO) "Installed in $(DESTDIR) with prefix $(prefix)"

install-kernel-indep:
	$(DIR) $(DESTDIR)$(initd_dir) $(DESTDIR)$(moduledir)/emc2 \
		$(DESTDIR)$(sysconfdir)/emc2 $(DESTDIR)$(bindir) \
		$(DESTDIR)/usr/local/etc/emc2/configs \
		$(DESTDIR)$(libdir) $(DESTDIR)$(includedir) \
		$(DESTDIR)$(docsdir)/help $(DESTDIR)$(ncfilesdir) \
		$(DESTDIR)/etc/X11/app-defaults $(DESTDIR)$(tcldir)/bin \
		$(DESTDIR)$(tcldir)/scripts $(DESTDIR)$(mandir)/man1 \
		$(DESTDIR)$(tcldir)/msgs \
		$(DESTDIR)$(localedir)/de/LC_MESSAGES
	$(FILE) ../docs/man/man1/halcmd.1 $(DESTDIR)$(mandir)/man1
	$(FILE) ../docs/man/man1/halui.1 $(DESTDIR)$(mandir)/man1
	$(FILE) ../docs/man/man1/halmeter.1 $(DESTDIR)$(mandir)/man1
	$(FILE) ../docs/man/man1/hal_joystick.1 $(DESTDIR)$(mandir)/man1
	$(FILE) ../docs/man/man1/emc.1 $(DESTDIR)$(mandir)/man1
	$(FILE) po/*.msg $(DESTDIR)$(tcldir)/msgs
	$(FILE) po/de*.mo $(DESTDIR)$(localedir)/de/LC_MESSAGES
	$(EXE) ../scripts/realtime $(DESTDIR)$(initd_dir)
	$(FILE) ../*.png ../*.gif $(DESTDIR)$(sysconfdir)/emc2
	$(INSTALL_CONFIGS)
	$(EXE) $(filter ../bin/%,$(TARGETS)) $(DESTDIR)$(bindir)
	$(EXE) ../scripts/emc $(DESTDIR)$(bindir)
	$(FILE) $(filter ../lib/%.a ../lib/%.so,$(TARGETS)) $(DESTDIR)$(libdir)
	-ldconfig $(DESTDIR)$(libdir)
	$(FILE) $(filter %.h %.hh,$(TARGETS)) $(DESTDIR)$(includedir)
	$(FILE) $(addprefix ../docs/,$(DOCS)) $(DESTDIR)$(docsdir)
	$(FILE) $(DOCS_HELP) $(DESTDIR)$(docsdir)/help
	$(FILE) $(NC_FILES) $(DESTDIR)$(ncfilesdir)
	$(EXE) ../nc_files/M101 $(DESTDIR)$(ncfilesdir)
	$(FILE) ../tcl/TkEmc $(DESTDIR)/etc/X11/app-defaults
	$(FILE) ../app-defaults/XEmc $(DESTDIR)/etc/X11/app-defaults
	$(FILE) Makefile.modinc $(DESTDIR)/etc/emc2
	$(EXE) $(TCL) $(DESTDIR)$(tcldir)
	$(EXE) $(TCL_BIN) $(DESTDIR)$(tcldir)/bin
	$(FILE) ../tcl/scripts/balloon.tcl ../tcl/scripts/emchelp.tcl $(DESTDIR)$(tcldir)/scripts
	$(EXE) ../tcl/scripts/Set_Coordinates.tcl $(DESTDIR)$(tcldir)/scripts

install-kernel-dep:
	$(FILE) ../rtlib/*$(MODULE_EXT) $(DESTDIR)$(moduledir)/emc2
	$(SETUID) ../bin/emc_module_helper $(DESTDIR)$(bindir)
	$(FILE) ../scripts/rtapi.conf $(DESTDIR)$(sysconfdir)/emc2
endif # RUN_IN_PLACE

CONF=../configs
COMMON=$(CONF)/common
CONFILES=$(addsuffix /$(1), $(filter-out $(COMMON), $(wildcard $(CONF)/*)))
.PHONY: configs
COPY_CONFIGS := \
	$(patsubst %,../configs/%/emc.nml, demo_mazak halui_halvcp hexapod-sim max \
		univpwm univstep stepper-xyza demo_step_cl stepper motenc m5i20 stg \
		sim vti nist-lathe dallur-thc) \
	$(patsubst %,../configs/%/core_axis.hal, demo_mazak halui_halvcp hexapod-sim max univpwm univstep \
		stepper-xyza demo_step_cl stepper motenc m5i20 stg sim vti nist-lathe) \
	$(patsubst %,../configs/%/core_stepper.hal, demo_step_cl stepper) \
	$(patsubst %,../configs/%/core_servo.hal, motenc m5i20 stg vti) \
	$(patsubst %,../configs/%/core_sim.hal, halui_halvcp sim) \
	../configs/sim/axis_manualtoolchange.hal
configs: $(COPY_CONFIGS)
	
$(call CONFILES,axis_manualtoolchange.hal): %/axis_manualtoolchange.hal: ../configs/common/axis_manualtoolchange.hal
	-cp $< $@
$(call CONFILES,emc.nml): %/emc.nml: ../configs/common/emc.nml
	-cp $< $@
$(call CONFILES,core_stepper.hal): %/core_stepper.hal: ../configs/common/core_stepper.hal
	-cp $< $@
$(call CONFILES,core_servo.hal): %/core_servo.hal: ../configs/common/core_servo.hal
	-cp $< $@
$(call CONFILES,core_sim.hal): %/core_sim.hal: ../configs/common/core_sim.hal
	-cp $< $@
$(call CONFILES,core_axis.hal): %/core_axis.hal: ../configs/common/core_axis.hal
	-cp $< $@
 
endif # userspace

# KERNELRELEASE is nonempty, therefore we are building modules using the
# "kbuild" system.  $(BASEPWD) is used here, instead of relative paths, because
# that's what kbuild seems to require

EXTRA_CFLAGS = $(RTFLAGS) -D__MODULE__ -I$(BASEPWD) -I$(BASEPWD)/libnml/linklist \
	-I$(BASEPWD)/libnml/cms -I$(BASEPWD)/libnml/rcs -I$(BASEPWD)/libnml/inifile \
	-I$(BASEPWD)/libnml/os_intf -I$(BASEPWD)/libnml/nml -I$(BASEPWD)/libnml/buffer \
	-I$(BASEPWD)/libnml/posemath -I$(BASEPWD)/rtapi -I$(BASEPWD)/hal \
	-I$(BASEPWD)/emc/nml_intf -I$(BASEPWD)/emc/kinematics -I$(BASEPWD)/emc/motion

ifeq "$(USE_STUBS)" "1"
MATHSTUB := rtapi/mathstubs.o
endif

ifdef SEQUENTIAL_SUPPORT
EXTRA_CFLAGS += -DSEQUENTIAL_SUPPORT
endif

# For each module, there's an addition to obj-m or obj-$(CONFIG_foo)
# plus a definition of foo-objs, which contains the full path to the
# object file(s) that the module contains.  Unfortunately, this setup pollutes
# the source directory with object files and other temporaries, but I can't
# find a way around it.

# Subdirectory:  rtapi
obj-$(CONFIG_RTAPI) += rtapi.o
rtapi-objs := rtapi/$(RTPREFIX)_rtapi.o

# Subdirectory: rtapi/examples (unneeded?)

# Subdirectory: hal/components
obj-$(CONFIG_BLOCKS) += blocks.o
blocks-objs := hal/components/blocks.o $(MATHSTUB)
obj-$(CONFIG_DEBOUNCE) += debounce.o
debounce-objs := hal/components/debounce.o $(MATHSTUB)
obj-$(CONFIG_ENCODER) += encoder.o
encoder-objs := hal/components/encoder.o $(MATHSTUB)
obj-$(CONFIG_COUNTER) += counter.o
counter-objs := hal/components/counter.o $(MATHSTUB)
obj-$(CONFIG_ENCODER_RATIO) += encoder_ratio.o
encoder_ratio-objs := hal/components/encoder_ratio.o $(MATHSTUB)
obj-$(CONFIG_STEPGEN) += stepgen.o
stepgen-objs := hal/components/stepgen.o $(MATHSTUB)
obj-$(CONFIG_FREQGEN) += freqgen.o
freqgen-objs := hal/components/freqgen.o $(MATHSTUB)
obj-$(CONFIG_PWMGEN) += pwmgen.o
pwmgen-objs := hal/components/pwmgen.o $(MATHSTUB)
obj-$(CONFIG_SIGGEN) += siggen.o
siggen-objs := hal/components/siggen.o $(MATHSTUB)
obj-$(CONFIG_PID) += pid.o
pid-objs := hal/components/pid.o $(MATHSTUB)
obj-$(CONFIG_PID) += threads.o
threads-objs := hal/components/threads.o $(MATHSTUB)
obj-$(CONFIG_SUPPLY) += supply.o
supply-objs := hal/components/supply.o $(MATHSTUB)
obj-$(CONFIG_TIMEDELAY) += timedelay.o
timedelay-objs := hal/components/timedelay.o $(MATHSTUB)
obj-$(CONFIG_SIM_ENCODER) += sim_encoder.o
sim_encoder-objs := hal/components/sim_encoder.o $(MATHSTUB)
obj-$(CONFIG_WEIGHTED_SUM) += weighted_sum.o
weighted_sum-objs := hal/components/weighted_sum.o $(MATHSTUB)
obj-$(CONFIG_MODMATH) += modmath.o
modmath-objs := hal/components/modmath.o $(MATHSTUB)

# Subdirectory: hal/drivers
obj-$(CONFIG_HAL_PARPORT) += hal_parport.o
hal_parport-objs := hal/drivers/hal_parport.o $(MATHSTUB)
obj-$(CONFIG_HAL_TIRO) += hal_tiro.o
hal_tiro-objs := hal/drivers/hal_tiro.o $(MATHSTUB)
obj-$(CONFIG_HAL_STG) += hal_stg.o
hal_stg-objs := hal/drivers/hal_stg.o $(MATHSTUB)
obj-$(CONFIG_HAL_VTI) += hal_vti.o
hal_vti-objs := hal/drivers/hal_vti.o $(MATHSTUB)
obj-$(CONFIG_HAL_EVOREG) += hal_evoreg.o
hal_evoreg-objs := hal/drivers/hal_evoreg.o $(MATHSTUB)
obj-$(CONFIG_HAL_MOTENC) += hal_motenc.o
hal_motenc-objs := hal/drivers/hal_motenc.o $(MATHSTUB)
obj-$(CONFIG_HAL_M5I20) += hal_m5i20.o
hal_m5i20-objs := hal/drivers/hal_m5i20.o $(MATHSTUB)
obj-$(CONFIG_HAL_AX521H) += hal_ax5214h.o
hal_ax5214h-objs := hal/drivers/hal_ax5214h.o $(MATHSTUB)
obj-$(CONFIG_HAL_PPMC) += hal_ppmc.o
hal_ppmc-objs := hal/drivers/hal_ppmc.o $(MATHSTUB)
obj-$(CONFIG_HAL_SPEAKER) += hal_speaker.o
hal_speaker-objs := hal/drivers/hal_speaker.o $(MATHSTUB)
obj-$(CONFIG_HAL_SKELETON) += hal_skeleton.o
hal_skeleton-objs := hal/drivers/hal_skeleton.o $(MATHSTUB)

ifneq "$(filter 2.6.%, $(kernelvers))" ""
obj-$(CONFIG_PROBE_PARPORT) += probe_parport.o
probe_parport-objs := hal/drivers/probe_parport.o $(MATHSTUB)
endif

obj-$(CONFIG_CLASSICLADDER_RT) += classicladder_rt.o
classicladder_rt-objs := hal/classicladder/module_rt.o $(MATHSTUB)
classicladder_rt-objs += hal/classicladder/calc_rt.o
classicladder_rt-objs += hal/classicladder/vars_access.o
classicladder_rt-objs += hal/classicladder/arithm_eval.o
classicladder_rt-objs += hal/classicladder/manager.o
classicladder_rt-objs += hal/classicladder/arrays.o

ifdef SEQUENTIAL_SUPPORT
classicladder_rt-objs += hal/classicladder/calc_sequential_rt.o
endif

obj-m += scope_rt.o
scope_rt-objs := hal/utils/scope_rt.o $(MATHSTUB)

obj-m += hal_lib.o
hal_lib-objs := hal/hal_lib.o $(MATHSTUB)

obj-m += trivkins.o
trivkins-objs := emc/kinematics/trivkins.o

obj-m += rotatekins.o
rotatekins-objs := emc/kinematics/rotatekins.o

obj-m += tripodkins.o
tripodkins-objs := emc/kinematics/tripodkins.o

obj-m += genhexkins.o
genhexkins-objs := emc/kinematics/genhexkins.o

obj-$(CONFIG_MOTMOD) += motmod.o
motmod-objs := emc/kinematics/cubic.o 
motmod-objs += emc/kinematics/mmxavg.o 
motmod-objs += emc/kinematics/tc.o 
motmod-objs += emc/kinematics/tp.o 
motmod-objs += emc/motion/motion.o 
motmod-objs += emc/motion/command.o 
motmod-objs += emc/motion/control.o 
motmod-objs += emc/motion/emcmotglb.o 
motmod-objs += emc/motion/emcmotutil.o 
motmod-objs += libnml/posemath/_posemath.o
motmod-objs += libnml/posemath/sincos.o $(MATHSTUB)

TORTOBJS = $(foreach file,$($(patsubst %.o,%,$(1))-objs), objects/rt$(file))
ifeq ($(BUILD_SYS),normal)
modules: $(patsubst %,../rtlib/%,$(obj-m))
RTOBJS := $(sort $(foreach mod,$(obj-m),$(call TORTOBJS,$(mod))))
RTDEPS := $(sort $(patsubst objects/%.o,depends/%.d, $(RTOBJS)))

$(sort $(RTDEPS)): depends/rt%.d: %.c
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CC),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(EXTRA_CFLAGS) $<)


# Rules to make .o (object) files
$(sort $(RTOBJS)) : objects/rt%.o : %.c
	$(ECHO) Compiling realtime $<
	@mkdir -p $(dir $@)
	$(CC) -c -DRTAPI -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(KDIR)/include $(EXTRA_CFLAGS) $< -o $@

../rtlib/%.o:
	$(ECHO) Linking $@
	@ld -r -static -S -O2 -o $@ $^ $(EXTRALINK) $(MATHLIB)

../rtlib/classicladder_rt: $(addprefix objects/rt,$(classicladder_rt-objs))
../rtlib/rtapi.o: $(addprefix objects/rt,$(rtapi-objs))
../rtlib/blocks.o: $(addprefix objects/rt,$(blocks-objs))
../rtlib/debounce.o: $(addprefix objects/rt,$(debounce-objs))
../rtlib/encoder.o: $(addprefix objects/rt,$(encoder-objs))
../rtlib/counter.o: $(addprefix objects/rt,$(counter-objs))
../rtlib/encoder_ratio.o: $(addprefix objects/rt,$(encoder_ratio-objs))
../rtlib/stepgen.o: $(addprefix objects/rt,$(stepgen-objs))
../rtlib/freqgen.o: $(addprefix objects/rt,$(freqgen-objs))
../rtlib/pwmgen.o: $(addprefix objects/rt,$(pwmgen-objs))
../rtlib/siggen.o: $(addprefix objects/rt,$(siggen-objs))
../rtlib/pid.o: $(addprefix objects/rt,$(pid-objs))
../rtlib/threads.o: $(addprefix objects/rt,$(threads-objs))
../rtlib/supply.o: $(addprefix objects/rt,$(supply-objs))
../rtlib/timedelay.o: $(addprefix objects/rt,$(timedelay-objs))
../rtlib/sim_encoder.o: $(addprefix objects/rt,$(sim_encoder-objs))
../rtlib/weighted_sum.o: $(addprefix objects/rt,$(weighted_sum-objs))
../rtlib/modmath.o: $(addprefix objects/rt,$(modmath-objs))
../rtlib/hal_parport.o: $(addprefix objects/rt,$(hal_parport-objs))
../rtlib/hal_tiro.o: $(addprefix objects/rt,$(hal_tiro-objs))
../rtlib/hal_stg.o: $(addprefix objects/rt,$(hal_stg-objs))
../rtlib/hal_vti.o: $(addprefix objects/rt,$(hal_vti-objs))
../rtlib/hal_evoreg.o: $(addprefix objects/rt,$(hal_evoreg-objs))
../rtlib/hal_motenc.o: $(addprefix objects/rt,$(hal_motenc-objs))
../rtlib/hal_m5i20.o: $(addprefix objects/rt,$(hal_m5i20-objs))
../rtlib/hal_ax5214h.o: $(addprefix objects/rt,$(hal_ax5214h-objs))
../rtlib/hal_ppmc.o: $(addprefix objects/rt,$(hal_ppmc-objs))
../rtlib/hal_skeleton.o: $(addprefix objects/rt,$(hal_skeleton-objs))
../rtlib/hal_speaker.o: $(addprefix objects/rt,$(hal_speaker-objs))
../rtlib/classicladder_rt.o: $(addprefix objects/rt,$(classicladder_rt-objs))
../rtlib/scope_rt.o: $(addprefix objects/rt,$(scope_rt-objs))
../rtlib/hal_lib.o: $(addprefix objects/rt,$(hal_lib-objs))
../rtlib/motmod.o: $(addprefix objects/rt,$(motmod-objs))
../rtlib/trivkins.o: $(addprefix objects/rt,$(trivkins-objs))
../rtlib/rotatekins.o: $(addprefix objects/rt,$(rotatekins-objs))
../rtlib/tripodkins.o: $(addprefix objects/rt,$(tripodkins-objs))
../rtlib/genhexkins.o: $(addprefix objects/rt,$(genhexkins-objs))

ifneq ($(MAKECMDGOALS),clean)
RTDEPS := $(sort $(patsubst objects/%.o,depends/%.d,$(RTOBJS)))
-include $(RTDEPS)
Makefile: $(RTDEPS)
endif
endif

# Phony so that it is always rebuilt when requested, not because it
# shouldn't exist as a file
.PHONY: tags
tags:
	ctags-exuberant -R . ../tcl ../scripts
