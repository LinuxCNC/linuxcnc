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



# TMPDIR is a temporary directory that can be used to
# create the tar file

TMPDIR=/tmp/emc2-build-pkg/
# Some day we may want to change the archive name to
# contain a version or a date..   that would be done on the next line
ARCHIVENAME=emc2



include Makefile.inc
#
# this rule handles most targets
# it simply changes to all the source sub-directories and calls make there
# note the order - low level code like rtapi is made first, before higher
# level code that might depend on it

SUBDIRS = src/rtapi src/hal src/libnml src/emc

all headers indent install depend clean:
	@@for subdir in $(SUBDIRS); \
	do \
		echo "Making $@ in $$subdir"; \
		make -C $$subdir $@ ; \
	done


# these variables are used to build a list of all
# man pages that need to be installed

ifneq ($(strip $(MAN_DIR)),)
# MAN_DIR exists, generate list of man pages
MAN1_FILES := $(patsubst docs/man/%,$(MAN_DIR)/%,$(wildcard docs/man/man1/*.1))
MAN3_FILES := $(patsubst docs/man/%,$(MAN_DIR)/%,$(wildcard docs/man/man3/*.3))
MAN_FILES = $(MAN1_FILES) $(MAN3_FILES)
else
# no man dir, do nothing
MAN_FILES =
endif

# this rule installs a single man page

$(MAN_DIR)/% : docs/man/%
	@ echo "install man page $*"
	@ cp $< $@

# this rule handles the uninstall target
# it removes all the man pages

uninstall :
	@ rm $(MAN_FILES)

# this rule handles the depend target
# it first updates the headers target to ensure that the header
# files are installed, then runs the depend target in each directory

depend : headers

# this rule handles the examples target
# it only enters directories that have examples
examples :
	(cd src/rtapi; make $@)

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.
clean :
	find . -name "*~" -exec rm -f {} \;
	find . -name "*.bak" -exec rm -f {} \;
	find . -name core -exec rm -f {} \;
	-(rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	(if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	(if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	(if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)
	rm -f $(ARCHIVENAME).tbz
	rm -f rpm_build_log
	rm -f rpm/emc2.spec


#
# Installation
#
# Installation is a big section for EMC, as it's a very complicated
# system...
#
# It's broken in to several different components, which
# all get built by install here:

install : install_man install_bin install_lib\
        install_modules install_scripts install_init \
        install_configs install_po




# BINARIES is a list of all of the binary executables to install
 
BINARIES = emcsvr hal_skeleton halmeter inivar milltask usrmot emcsh hal_parport halcmd halscope iosh simio


# CONFIGS is a list of all of the configuration files we install...

CONFIGS=emc.conf hal.conf rtapi.conf core_stepper.hal emc.ini emc.nml emc.var simulated_limits.hal standard_pinout.hal xylotex_pinout.hal TkEmc

# The target install location for configuration files:

CONFIGDIR=$(sysconfdir)

# install:
# installs the EMC system
# use DESTDIR=[chroot location] to allow for package building
# into a root path other than /


install_bin: 
	install -d $(DESTDIR)$(TESTDIR)/$(bindir)

	@@for file in $(BINARIES); \
	do \
		echo "Installing $$file"; \
		cp bin/$$file $(DESTDIR)$(TESTDIR)/$(bindir); \
	done

install_lib: 
	install -d $(DESTDIR)$(TESTDIR)/$(libdir)
	cp lib/*.o lib/*.a $(DESTDIR)$(TESTDIR)/$(libdir)
	@ echo "lib installed"

# install_scripts:
# scripts are copied, and reformated before being installed
# with this target.  The "$TESTDIR" environment variable that
# is used in the scripts gets replaced with an absolute path
# to the install location.

# SCRIPTS is a list of all of the scripts to be installed

SCRIPTS = scripts/emc2.run scripts/hal_demo 
TCL_SCRIPTS = tcl/tkemc.tcl \
tcl/bin/emcdebug.tcl tcl/bin/emctesting.tcl tcl/bin/genedit.tcl \
tcl/bin/tkio.tcl tcl/bin/emccalib.tcl tcl/bin/emclog.tcl \
tcl/bin/emctuning.tcl tcl/bin/tkbackplot.tcl \
tcl/scripts/DIO_Exercise.tcl tcl/scripts/IO_Exercise.tcl \
tcl/scripts/IO_Show.tcl tcl/scripts/balloon.tcl tcl/scripts/emchelp.tcl

install_scripts:
	install -d $(DESTDIR)$(TESTDIR)/$(bindir)

	cp $(SCRIPTS) $(DESTDIR)$(TESTDIR)/$(bindir)
	@@for script in $(TCL_SCRIPTS); \
	do \
		echo "Transfering $$script"; \
		DIR="`dirname $$script`"; \
		install -d $(DESTDIR)$(TESTDIR)/$(bindir)/$$DIR ; \
		cp $$script $(DESTDIR)$(TESTDIR)/$(bindir)/$$DIR ; \
	done


# install_configs:
# Copy and alter the configuration files
# Replacements similar to those done on scripts occur ot the config
# files.

install_configs:
	@ echo "Installing configs..."
	install -d $(DESTDIR)$(TESTDIR)$(CONFIGDIR)
	
	@@for config in $(CONFIGS); \
	do \
		echo "Creating $$config"; \
		cat configs/$$config | sed "s%\$$TESTDIR%$(TESTDIR)%;s%\$$EMC2CONFIGDIR%$(TESTDIR)$(CONFIGDIR)%;" > $(DESTDIR)$(TESTDIR)$(CONFIGDIR)/$$config; \
	done

	@ echo "configs installed"

# install_rt_modules:
# Installs the realtime modules from rtlib in the module directory
# (Also installs .runinfo, which is still a bit of a mystery to me)
# [If we handle the loading/unloading of all kernel modules, .runinfo
# can be ignored - It is an RTAI only thing]

install_rt_modules:
	install -d $(DESTDIR)$(TESTDIR)/$(moduledir)
	cp rtlib/*.o rtlib/*.a $(DESTDIR)$(TESTDIR)/$(moduledir)
	cp scripts/.runinfo $(DESTDIR)$(TESTDIR)/$(moduledir)

# modules_install or install_modules:
# Installs all of the kernel modules

modules_install install_modules: install_rt_modules 
	@ echo "modules installed"

# install_init:
# Installs the realtime init script in /etc/rc.d/init.d

install_init:
	install -d $(DESTDIR)$(TESTDIR)/etc/rc.d/init.d/
	cat scripts/realtime | sed "s%\$$EMC_RTAPICONF%$(TESTDIR)$(CONFIGDIR)/rtapi.conf%;" > $(DESTDIR)$(TESTDIR)/etc/rc.d/init.d/realtime
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
		cp po/$$file $(DESTDIR)$(TESTDIR)/$(localedir); \
	done
	

install_man: $(MAN_FILES)
	install -d $(DESTDIR)$(TESTDIR)$(mandir)/man1
	install -d $(DESTDIR)$(TESTDIR)$(mandir)/man3
	@ echo "install man pages"

	@@for file in $(MAN1_FILES); \
	do \
		echo "Copying $$file to man1"; \
		cp $$file $(DESTDIR)$(TESTDIR)$(mandir)/man1 ; \
	done

	@@for file in $(MAN3_FILES); \
	do \
		echo "Copying $$file to man3"; \
		cp $$file $(DESTDIR)$(TESTDIR)$(mandir)/man3 ; \
	done








# create an archive of the current source
# this is needed for rpm, and can also be used to create
# snapshots for download by those who can't or don't want
# to use CVS


$(ARCHIVENAME).tbz: clean
	rm -rf $(TMPDIR)
	install -d $(TMPDIR)/$(ARCHIVENAME)

	cp -R ./ $(TMPDIR)/$(ARCHIVENAME)
	tar -cjf $(ARCHIVENAME).tbz -C $(TMPDIR) $(ARCHIVENAME)
	rm -rf $(TMPDIR)

tbz: $(ARCHIVENAME).tbz

rpm: $(ARCHIVENAME).tbz rpm/emc2.spec
	sudo cp $(ARCHIVENAME).tbz /usr/src/redhat/SOURCES/emc2.tar.bz2

	date > rpm_build_log
	(sudo rpmbuild -ba -v rpm/emc2.spec 2>&1) | tee -a rpm_build_log
	date >> rpm_build_log

# If Makerfile.inc doesn't exist yet, run configure...
Makefile.inc:
	./configure

rpm/emc2.spec:
	./configure


.PHONY : all examples headers depend indent install clean
