# top level Makefile
# -------------
#
# Notes on syntax:
# @ causes each command NOT to be echoed to the console.
# - allows the rule to continue after an error.
#
#
# Notes on targets:
#
# all - default target, makes the whole thing except examples
# examples - makes rtapi examples, run after "make all"
# headers - copies public header files to INC_DIR (emc2/include)
# depend - generate dependency file(s)
# indent - formats source code
# install - installs emc2 files to system directories
# uninstall - removes emc2 files from system directories
# install - right now this does nothing
# clean - cleans up temp files, backups, object files, binaries, etc.
#
# Note that right now the install and uninstall targets only install
# and remove man pages - the rest of emc2 lives entirely within the
# emc2 tree.  This may change later.
#
# Makefile.inc contains directory paths and other system dependent stuff
include Makefile.inc
#
# this rule handles most targets
# it simply changes to all the source sub-directories and calls make there
# note the order - low level code like rtapi is made first, before higher
# level code that might depend on it


LOCALDIR = `pwd`

# SUBDIRS is a list of all of the sub directories that make should
# look at when compiling

SUBDIRS = src/rtapi src/hal src/libnml src/emc

# SCRIPTS is a list of all of the scripts to be installed

SCRIPTS = emc.run hal_demo 

# BINARIES is a list of all of the binary executables to install
 
BINARIES = emcsvr hal_skeleton halmeter inivar milltask usrmot emcsh hal_parport halcmd halscope iosh simio


# The list of all of the configuration files we install...

CONFIGS=emc.conf hal.conf rtapi.conf core_stepper.hal emc.ini emc.nml emc.var simulated_limits.hal standard_pinout.hal xylotex_pinout.hal

# The target install location for configuration files:

CONFIGTARGET=$(DESTDIR)$(TESTDIR)/$(sysconfdir)

# The expected location of the emc.conf configuration file:

EMCCONFIG=$(CONFIGTARGET)/emc.conf

# install:
# installs the EMC system
# use DESTDIR=[chroot location] to allow for package building
# into a root path other than /

all headers indent install depend clean:
	@@for subdir in $(SUBDIRS); \
	do \
		echo "Making $@ in $$subdir"; \
		make -C $$subdir $@ ; \
	done


# these variables are used to build a list of all
# man pages that need to be installed

ifneq ($(strip $(mandir)),)
# MAN_DIR exists, generate list of man pages
MAN1_FILES := $(patsubst docs/man/%,$(DESTDIR)$(TESTDIR)$(mandir)/%,$(wildcard docs/man/man1/*.1))
MAN3_FILES := $(patsubst docs/man/%,$(DESTDIR)$(TESTDIR)$(mandir)/%,$(wildcard docs/man/man3/*.3))
MAN_FILES = $(MAN1_FILES) $(MAN3_FILES)
else
# no man dir, do nothing
MAN_FILES =
endif

# this rule installs a single man page

man_directories:
	install -d $(DESTDIR)$(TESTDIR)$(mandir)/man1
	install -d $(DESTDIR)$(TESTDIR)$(mandir)/man3

$(DESTDIR)$(TESTDIR)$(mandir)/% : docs/man/%
	@ echo "install man page $*"
	@ cp $< $@


# Install the man pages:

install_man: man_directories $(MAN_FILES)

# install_bin:
# Copy and install all of the binaries (listed in BINARIES,
# and found in the various script directories) into their install
# location

install_bin: 
	install -d $(DESTDIR)$(TESTDIR)/$(bindir)

	@@for file in $(BINARIES); \
	do \
		echo "Installing $$file"; \
		cp bin/$$file $(DESTDIR)$(TESTDIR)/$(bindir); \
	done

	#cp -R bin/* $(DESTDIR)$(TESTDIR)/$(bindir)
	cp tcl/*.tcl $(DESTDIR)$(TESTDIR)/$(bindir)
	cp tcl/bin/*.tcl $(DESTDIR)$(TESTDIR)/$(bindir)
	cp tcl/scripts/*.tcl $(DESTDIR)$(TESTDIR)/$(bindir)

# install_bin:
# When we get EMC to where it can be run by normal users, and not
# just root, the root-level binaries will go here:

install_sbin:
#	install -d $(DESTDIR)$(TESTDIR)/$(sbindir)
	@ echo "sbin installed"

# install_info:
# If and when we move to doxygen or some other automated documetning
# system that can produce info files, they'll be installed in this target

install_info:
#	install -d $(DESTDIR)$(TESTDIR)/$(infodir)
	@ echo "info installed"

# install_lib:
# libraries get copied into their target location with this target

install_lib: 
	install -d $(DESTDIR)$(TESTDIR)/$(libdir)
	cp lib/*.o lib/*.a $(DESTDIR)$(TESTDIR)/$(libdir)
	@ echo "lib installed"

# install_scripts:
# scripts are copied, and reformated before being installed
# with this target.  The "$TESTDIR" environment variable that
# is used in the scripts gets replaced with an absolute path
# to the install location.

install_scripts:
	install -d $(DESTDIR)$(TESTDIR)/$(bindir)
#	(cd scripts ; cp -r $(SCRIPTS) $(DESTDIR)/$(bindir))
	
	@@for script in $(SCRIPTS); \
	do \
		echo "Creating $$script"; \
		cat scripts/$$script | sed "s%\$$TESTDIR%$(TESTDIR)%;s%\$$EMCCONFIG%$(EMCCONFIG)%;" > $(DESTDIR)$(TESTDIR)/$(bindir)/$$script; \
		chmod a+x $(DESTDIR)$(TESTDIR)/$(bindir)/$$script; \
	done


# install_configs:
# Copy and alter the configuration files
# Replacements similar to those done on scripts occur ot the config
# files.

install_configs:
	@ echo "Installing configs..."
	install -d $(CONFIGTARGET)
#	(cd scripts ; cp -r $(SCRIPTS) $(DESTDIR)/$(bindir))
	
	@@for config in $(CONFIGS); \
	do \
		echo "Creating $$config"; \
		cat configs/$$config | sed "s%\$$TESTDIR%$(TESTDIR)%;s%\$$EMCCONFIG%$(EMCCONFIG)%;" > $(CONFIGTARGET)/$$config; \
	done

	@ echo "configs installed"


# install_hal_module:
# Installs hal kernel modules into their proper module directory
# FIXME: There aren't any modules currently marked as HAL modules
# John Kasunich has epxressed a preference to keep hal modules seperate
# from emc modules

install_hal_modules:
	install -d $(DESTDIR)$(TESTDIR)/$(halmoduledir)
#	cp $(DESTDIR)/$(halmoduledir)

# install_rt_modules:
# Installs the realtime modules from rtlib in the module directory
# (Also installs .runinfo, which is still a bit of a mystery to me)

install_rt_modules:
	install -d $(DESTDIR)$(TESTDIR)/$(moduledir)
	cp rtlib/*.o rtlib/*.a $(DESTDIR)$(TESTDIR)/$(moduledir)
	cp scripts/.runinfo $(DESTDIR)$(TESTDIR)/$(moduledir)

# modules_install or install_modules:
# Installs all of the kernel modules

modules_install install_modules: install_rt_modules install_hal_modules
	@ echo "modules installed"

# install_init:
# Installs the realtime init script in /etc/rc.d/init.d

install_init:
	install -d $(DESTDIR)$(TESTDIR)/etc/rc.d/init.d/
	cat scripts/realtime | sed "s%\$$EMC_RTAPICONF%$(CONFIGTARGET)/rtapi.conf%;" > $(DESTDIR)$(TESTDIR)/etc/rc.d/init.d/realtime
	chmod a+x $(DESTDIR)$(TESTDIR)/etc/rc.d/init.d/realtime
	@ echo "Realtime script installed"

# install_po:
# install language speciifc po files into their directory
# The list of all supported i18n languages that have .po files
# is defined in the Maekfile.inc as LANGUAGES

install_po:
	install -d $(DESTDIR)$(TESTDIR)/$(localedir)
	@@for file in $(LANGUAGES); \
	do \
		echo "Installing $$file"; \
		cp lang/$$file $(DESTDIR)$(TESTDIR)/$(localedir); \
	done
	
# this rule handles the install target
# its dependency installs all the man pages

install : install_man install_bin install_lib\
	install_modules install_scripts install_init install_sbin\
	install_info install_configs install_po

# this rule handles the uninstall target
# it removes all the man pages
# TODO: make it uninstall the rest...

uninstall :
	@ rm $(MAN_FILES)

# this rule handles the depend target
# it first updates the headers target to ensure that the header
# files are installed, then runs the depend target in each directory

depend : headers

# this rule handles the examples target
# it only enters directories that have examples
examples :
	make -C src/rtapi $@

# topclean:
# cleans the top level 
topclean :
	find . -name "*~" -exec rm -f {} \;
	find . -name "*.bak" -exec rm -f {} \;
	find . -name core -exec rm -f {} \;
	-(rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	(if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	(if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	(if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)
	rm -rf ./test

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.

clean: topclean

# make test:
# This tests emc.
# For now, it installs into a test directory
# TODO: Add real tests for the subsystems

test:
	make install TESTDIR=$(LOCALDIR)/test

# make run:
# make run is the 1 stop make target for emc.
# TOOD: this will go away as soon as I figure out how to make
# run-in-place work again.

run: all test
	./test/$(bindir)/emc.run

# make fresh:
# utility target that tests out the build scripts.
# TODO: this will go away as soon as run-in-place gets fixed

fresh:
	rm -rf test
	./configure
	make run


.PHONY : all examples headers depend indent install clean
