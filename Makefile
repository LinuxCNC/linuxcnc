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

LOCALDIR = $(EMC2_HOME)

all headers indent :
	chmod +x scripts/emc.run
	(cd src/rtapi; make $@)
	(cd src/hal; make $@)
	(cd src/libnml; make $@)
	(cd src/emc; make $@)

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

install : all $(MAN_FILES)
	
	# waiting for emc to be "rock solid" (quote by P.C.)
	cp scripts/realtime $(initd_dir)/
	@ echo "realtime script installed"
	
	install -d $(moduledir)
	cp -R rtlib/*.o $(moduledir)
	cp scripts/.runinfo $(moduledir)
	@ echo "kernel modules installed"
	
	install -d /etc/emc2
	cp -R configs/*.conf /etc/emc2
	cp configs/*.hal /etc/emc2
	cp configs/emc.conf.install /etc/emc2/emc.conf
	cp configs/hal.conf.install /etc/emc2/hal.conf
	#cp configs/emc.conf.install /etc/emc2/emc.conf
	install -d $(prefix)/etc
	cp configs/*.ini configs/*.var configs/*.nml /etc/emc2
	@ echo "configs installed"
	
	install -d $(prefix)/bin
	cp -R bin/* $(prefix)/bin
	cp scripts/emc.run $(prefix)/bin
	@ echo "bin installed"
	
	install -d $(prefix)/lib
	(if (test -f lib/*.so) ; then cp lib/*.so $(prefix)/lib; fi)
	-(rm -f lib/*.a lib/*.o 2>/dev/null)
	@ echo "lib installed"
	
	install -d $(prefix)/share/emc2/docs
	cp -R docs/* $(prefix)/share/emc2/docs
	@ echo "docs installed"
	
	install -d $(prefix)/share/emc2/nc_files
	cp -R docs/* $(prefix)/share/emc2/nc_files
	@ echo "nc_files installed"
	
	install -d $(prefix)/share/emc2/tcl
	cp -R docs/* $(prefix)/share/emc2/tcl
	@ echo "tcl installed"
	
	#install -d /usr/share/locale/$(locale)/LC_MESSAGES
	#@ echo "locale installed"

# this rule handles the uninstall target
# it removes all the man pages

uninstall :
	@ rm $(MAN_FILES)

# this rule handles the depend target
# it first updates the headers target to ensure that the header
# files are installed, then runs the depend target in each directory

depend : headers
	(cd src/rtapi; make $@)
	(cd src/hal; make $@)
	(cd src/libnml; make $@)
	(cd src/emc; make $@)

# this rule handles the examples target
# it only enters directories that have examples
examples :
	(cd src/rtapi; make $@)

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.
clean :
	(cd src/rtapi; make $@)
	(cd src/hal; make $@)
	(cd src/libnml; make $@)
	(cd src/emc; make $@)
	find . -name "*~" -exec rm -f {} \;
	find . -name "*.bak" -exec rm -f {} \;
	find . -name core -exec rm -f {} \;
	-(rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	(if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	(if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	(if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)


.PHONY : all examples headers depend indent install clean
