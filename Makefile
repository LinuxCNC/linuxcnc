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

# this rule handles the install target
# its dependency installs all the man pages

install_man: $(MAN_FILES)
	install -d $(DESTDIR)/$(mandir)
	cp $(MAN_FILES) $(DESTDIR)/$(mandir)

install_bin: 
	install -d $(DESTDIR)/$(bindir)
	cp bin/* $(DESTDIR)/$(bindir)

install_lib: 
	install -d $(DESTDIR)/$(libdir)
	cp lib/* $(DESTDIR)/$(libdir)

install_configs:
#	install -d $(DESTDIR)/$(sysconfdir)
#	cp -r configs/* $(DESTDIR)/$(sysconfdir)
	install -d $(DESTDIR)/$(prefix)/configs
	cp -r configs/* $(DESTDIR)/$(prefix)/configs

install_scripts:
	install -d $(DESTDIR)/$(prefix)/scripts
	cp -r scripts/* $(DESTDIR)/$(prefix)/scripts

MODPATH=$(RTDIR)/modules 

install_modules:
	mod_dir=$$(find /lib/modules/`uname -r` -name "mbuff.o" -o -name "rtai_shm.o") ; 
	
	install -d $(DESTDIR)/$(MODPATH) ;
	cp rtlib/* $(DESTDIR)/$(MODPATH) ;

install : install_man install_bin install_lib install_configs \
	install_modules install_scripts



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
	make -C src/rtapi $@

# this rule handles the clean target
# it changes to all the source sub-directories, calls make there, and
# then returns to the top level directory and cleans that up too.

topclean :
	find . -name "*~" -exec rm -f {} \;
	find . -name "*.bak" -exec rm -f {} \;
	find . -name core -exec rm -f {} \;
	-(rm -f include/* lib/* rtlib/* bin/* 2>/dev/null)
	(if [ -d $(TMP_DIR) ] ; then rm -fR $(TMP_DIR) ; fi)
	(if [ -d $(RTTMP_DIR) ] ; then rm -fR $(RTTMP_DIR) ; fi)
	(if [ -d $(GTKTMP_DIR) ] ; then rm -fR $(GTKTMP_DIR) ; fi)

clean: topclean

.PHONY : all examples headers depend indent install clean
