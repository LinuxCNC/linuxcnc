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
#           right now this is commented out (uncomment it if you want to test)
# install_rtscript - this installs the realtime script to /etc/init.d/ and rtapi.conf to /etc
# install_modules - installed the modules to the modulesdir (found by config)
# install_configs - installs the configs to $sysconfdir (usually /usr/local/etc)
# install_bin - installs bins to $bindir (usually /usr/local/bin)
# install_lib - installs libs to $libdir
# install_docs - installs docs to $docdir
# install_ncfiles - installs ncfiles 
# install_tcl - installs tcl code
# install_man - installs man pages
# uninstall - removes emc2 files from system directories
#           - FIXME - not done yet (will need a list of installed files)
# clean - cleans up temp files, backups, object files, binaries, etc.
# tgz - creates emc2.tar.gz containing the sources (does a clean first)
#       Usefull to produce an tarball of the latest CVS
# deb - FIXME - this should produce a deb package using the make install rule
# rpm - FIXME - this should produce a rpm package using the make install rule
#
#
# Makefile.inc contains directory paths and other system dependent stuff
include Makefile.inc

HEADERS = config.h

#
# this rule handles most targets
# it simply changes to all the source sub-directories and calls make there
# note the order - low level code like rtapi is made first, before higher
# level code that might depend on it

all headers indent : def_headers
	chmod +x ../scripts/emc
	(cd rtapi; make headers)
	(cd libnml; make headers)
	(cd emc; make headers)
	(cd hal; make headers)
	(cd rtapi; make $@)
	(cd libnml; make $@)
	(cd hal; make $@)
	(cd emc; make $@)

# Generic rules
include Make.rules

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

# this rule handles the install target
# its dependency installs all the man pages

install_rtscript:
	install -d $(DESTDIR)$(initd_dir)
	cp ../scripts/realtime $(DESTDIR)$(initd_dir)/
	#@ echo "realtime script installed"

install_modules:
	install -d $(DESTDIR)$(moduledir)
	-cp -f ../rtlib/*$(MODULE_EXT) $(DESTDIR)$(moduledir)
	cp -f ../rtlib/.runinfo $(DESTDIR)$(moduledir)
	@ echo "kernel modules installed"

install_configs:
	install -d $(DESTDIR)$(sysconfdir)
	cp ../emc2.gif $(DESTDIR)$(sysconfdir)
	cp -rf ../configs/* $(DESTDIR)$(sysconfdir)
	@ echo "configs installed"

install_bin:
	install -d $(DESTDIR)$(bindir)
	-cp -f ../bin/* $(DESTDIR)$(bindir)
	cp ../scripts/emc $(DESTDIR)$(bindir)
	chmod +x $(DESTDIR)$(bindir)/emc
	@ echo "bin installed"

install_lib:
	install -d $(DESTDIR)$(libdir)
	(if (test -f ../lib/*.so) ; then cp ../lib/*.so $(DESTDIR)$(libdir); fi)
	-(rm -f ../lib/*.a ../lib/*.o 2>/dev/null)
	@ echo "lib installed"

install_docs:
	install -d $(DESTDIR)$(docsdir)
	-cp -f ../docs/* $(DESTDIR)$(docsdir)
	@ echo "docs installed"

# to be added, when there is a nc_files dir
install_ncfiles:
	install -d $(DESTDIR)$(ncfilesdir)
	-GLOBIGNORE=CVS && cp -f ../nc_files/* $(DESTDIR)$(ncfilesdir)
	@ echo "nc_files installed"

install_tcl:
	install -d $(DESTDIR)$(tcldir)
	install -d $(DESTDIR)$(tcldir)/bin
	install -d $(DESTDIR)$(tcldir)/scripts
	-cp -f ../tcl/* $(DESTDIR)$(tcldir)
	-cp -f ../tcl/bin/* $(DESTDIR)$(tcldir)/bin/
	-cp -f ../tcl/scripts/* $(DESTDIR)$(tcldir)/scripts/
	@ echo "tcl installed"
	
	#install -d /usr/share/locale/$(locale)/LC_MESSAGES
	#@ echo "locale installed"

install_man : $(MAN_FILES)
	(cd rtapi; make $@)
	(cd hal; make $@)

#if you want to test, uncomment the following line (not safe till further testing)
#install: install_rtscript install_modules install_configs\
#	install_bin install_lib install_docs install_ncfiles\
#	install_tcl
#	(cd rtapi; make $@)

modules: headers
	(cd rtapi; make $@)
	(cd hal; make $@)
	(cd emc; make $@)

ifeq ($(BUILD_SYS),kbuild)

modules_install: modules
	(cd rtapi; make $@)
	(cd hal; make $@)
	(cd emc; make $@)
	depmod 2>/dev/null

endif

# this rule handles the uninstall target
# it removes all the man pages

uninstall :
	@ rm $(MAN_FILES)

# this rule handles the depend target
# it first updates the headers target to ensure that the header
# files are installed, then runs the depend target in each directory

depend : headers
	(cd rtapi; make $@)
	(cd hal; make $@)
	(cd libnml; make $@)
	(cd emc; make $@)

# this rule handles the examples target
# it only enters directories that have examples
examples :
	(cd rtapi; make $@)

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.
clean :
	(cd rtapi; make $@)
	(cd hal; make $@)
	(cd libnml; make $@)
	(cd emc; make $@)
	find . -name "*~" -exec rm -f {} \;
	find . -name "*.bak" -exec rm -f {} \;
	find . -name core -exec rm -f {} \;
	-(rm -f ../include/* ../lib/* ../rtlib/* ../bin/* 2>/dev/null)
	(if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	(if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	(if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)

tgz: clean
	rm -rf /tmp/emc2-tgz-temp/emc2
	install -d /tmp/emc2-tgz-temp/emc2/
	cp -R ../ /tmp/emc2-tgz-temp/emc2/

# Remove Makefile.inc to force a rerun of ./configure
# on the target machine where emc2.tar.gz will be copied to
	rm -f /tmp/emc2-tgz-temp/emc2/src/Makefile.inc
	rm -f /tmp/emc2-tgz-temp/emc2/src/config.status
	rm -f /tmp/emc2-tgz-temp/emc2/src/config.log
	rm -f /tmp/emc2-tgz-temp/emc2/emc2.tar.gz
	cd /tmp/emc2-tgz-temp ; tar -czf emc2.tar.gz emc2 --exclude *CVS*
	mv /tmp/emc2-tgz-temp/emc2.tar.gz ./
	rm -rf /tmp/emc2-tgz-temp

docs:
	doxygen doxconfig

.PHONY : all examples headers depend indent modules install clean
